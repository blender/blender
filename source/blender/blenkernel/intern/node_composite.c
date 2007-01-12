/**
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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h" /* qdn: defocus node, need camera info */
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_vec_types.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"		/* <- TexResult */
#include "RE_render_ext.h"		/* <- ibuf_sample() */

#ifndef atanf
	#define atanf(a)	atan((double)(a))
#endif


/* *************************** operations support *************************** */

/* general signal that's in output sockets, and goes over the wires */
typedef struct CompBuf {
	float *rect;
	int x, y, xrad, yrad;
	short type, malloc;
	rcti disprect;		/* cropped part of image */
	int xof, yof;		/* relative to center of target image */
	
	void (*rect_procedural)(struct CompBuf *, float *, float, float);
	bNode *node;		/* only in use for procedural bufs */
	
	struct CompBuf *next, *prev;	/* for pass-on, works nicer than reference counting */
} CompBuf;

/* defines also used for pixel size */
#define CB_RGBA		4
#define CB_VEC4		4
#define CB_VEC3		3
#define CB_VEC2		2
#define CB_VAL		1

/* defines for RGBA channels */
#define CHAN_R	0
#define CHAN_G	1
#define CHAN_B	2
#define CHAN_A	3

static CompBuf *alloc_compbuf(int sizex, int sizey, int type, int alloc)
{
	CompBuf *cbuf= MEM_callocN(sizeof(CompBuf), "compbuf");
	
	cbuf->x= sizex;
	cbuf->y= sizey;
	cbuf->xrad= sizex/2;
	cbuf->yrad= sizey/2;
	
	cbuf->type= type;
	if(alloc) {
		if(cbuf->type==CB_RGBA)
			cbuf->rect= MEM_mapallocN(4*sizeof(float)*sizex*sizey, "compbuf RGBA rect");
		else if(cbuf->type==CB_VEC3)
			cbuf->rect= MEM_mapallocN(3*sizeof(float)*sizex*sizey, "compbuf Vector3 rect");
		else if(cbuf->type==CB_VEC2)
			cbuf->rect= MEM_mapallocN(2*sizeof(float)*sizex*sizey, "compbuf Vector2 rect");
		else
			cbuf->rect= MEM_mapallocN(sizeof(float)*sizex*sizey, "compbuf Fac rect");
		cbuf->malloc= 1;
	}
	cbuf->disprect.xmin= 0;
	cbuf->disprect.ymin= 0;
	cbuf->disprect.xmax= sizex;
	cbuf->disprect.ymax= sizey;
	
	return cbuf;
}

static CompBuf *dupalloc_compbuf(CompBuf *cbuf)
{
	CompBuf *dupbuf= alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 1);
	if(dupbuf) {
		memcpy(dupbuf->rect, cbuf->rect, cbuf->type*sizeof(float)*cbuf->x*cbuf->y);
	
		dupbuf->xof= cbuf->xof;
		dupbuf->yof= cbuf->yof;
	}	
	return dupbuf;
}

/* instead of reference counting, we create a list */
static CompBuf *pass_on_compbuf(CompBuf *cbuf)
{
	CompBuf *dupbuf= alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 0);
	CompBuf *lastbuf;
	
	if(dupbuf) {
		dupbuf->rect= cbuf->rect;
		dupbuf->xof= cbuf->xof;
		dupbuf->yof= cbuf->yof;
		dupbuf->malloc= 0;
		
		/* get last buffer in list, and append dupbuf */
		for(lastbuf= dupbuf; lastbuf; lastbuf= lastbuf->next)
			if(lastbuf->next==NULL)
				break;
		lastbuf->next= dupbuf;
		dupbuf->prev= lastbuf;
	}	
	return dupbuf;
}


void free_compbuf(CompBuf *cbuf)
{
	/* check referencing, then remove from list and set malloc tag */
	if(cbuf->prev || cbuf->next) {
		if(cbuf->prev)
			cbuf->prev->next= cbuf->next;
		if(cbuf->next)
			cbuf->next->prev= cbuf->prev;
		if(cbuf->malloc) {
			if(cbuf->prev)
				cbuf->prev->malloc= 1;
			else
				cbuf->next->malloc= 1;
			cbuf->malloc= 0;
		}
	}
	
	if(cbuf->malloc && cbuf->rect)
		MEM_freeN(cbuf->rect);

	MEM_freeN(cbuf);
}

void print_compbuf(char *str, CompBuf *cbuf)
{
	printf("Compbuf %s %d %d %p\n", str, cbuf->x, cbuf->y, cbuf->rect);
	
}

static CompBuf *get_cropped_compbuf(rcti *drect, float *rectf, int rectx, int recty, int type)
{
	CompBuf *cbuf;
	rcti disprect= *drect;
	float *outfp;
	int dx, y;
	
	if(disprect.xmax>rectx) disprect.xmax= rectx;
	if(disprect.ymax>recty) disprect.ymax= recty;
	if(disprect.xmin>= disprect.xmax) return NULL;
	if(disprect.ymin>= disprect.ymax) return NULL;
	
	cbuf= alloc_compbuf(disprect.xmax-disprect.xmin, disprect.ymax-disprect.ymin, type, 1);
	outfp= cbuf->rect;
	rectf += type*(disprect.ymin*rectx + disprect.xmin);
	dx= type*cbuf->x;
	for(y=cbuf->y; y>0; y--, outfp+=dx, rectf+=type*rectx)
		memcpy(outfp, rectf, sizeof(float)*dx);
	
	return cbuf;
}

static CompBuf *scalefast_compbuf(CompBuf *inbuf, int newx, int newy)
{
	CompBuf *outbuf; 
	float *rectf, *newrectf, *rf;
	int x, y, c, pixsize= inbuf->type;
	int ofsx, ofsy, stepx, stepy;
	
	if(inbuf->x==newx && inbuf->y==newy)
		return dupalloc_compbuf(inbuf);
	
	outbuf= alloc_compbuf(newx, newy, inbuf->type, 1);
	newrectf= outbuf->rect;
	
	stepx = (65536.0 * (inbuf->x - 1.0) / (newx - 1.0)) + 0.5;
	stepy = (65536.0 * (inbuf->y - 1.0) / (newy - 1.0)) + 0.5;
	ofsy = 32768;
	
	for (y = newy; y > 0 ; y--){
		rectf = inbuf->rect;
		rectf += pixsize * (ofsy >> 16) * inbuf->x;

		ofsy += stepy;
		ofsx = 32768;
		
		for (x = newx ; x>0 ; x--) {
			
			rf= rectf + pixsize*(ofsx >> 16);
			for(c=0; c<pixsize; c++)
				newrectf[c] = rf[c];
			
			newrectf+= pixsize;
			
			ofsx += stepx;
		}
	}
	
	return outbuf;
}

static CompBuf *typecheck_compbuf(CompBuf *inbuf, int type)
{
	if(inbuf && inbuf->type!=type && inbuf->rect_procedural==NULL) {
		CompBuf *outbuf= alloc_compbuf(inbuf->x, inbuf->y, type, 1); 
		float *inrf= inbuf->rect;
		float *outrf= outbuf->rect;
		int x= inbuf->x*inbuf->y;
		
		/* warning note: xof and yof are applied in pixelprocessor, but should be copied otherwise? */
		outbuf->xof= inbuf->xof;
		outbuf->yof= inbuf->yof;
		
		if(type==CB_VAL) {
			if(inbuf->type==CB_VEC2) {
				for(; x>0; x--, outrf+= 1, inrf+= 2)
					*outrf= 0.5f*(inrf[0]+inrf[1]);
			}
			else if(inbuf->type==CB_VEC3) {
				for(; x>0; x--, outrf+= 1, inrf+= 3)
					*outrf= 0.333333f*(inrf[0]+inrf[1]+inrf[2]);
			}
			else if(inbuf->type==CB_RGBA) {
				for(; x>0; x--, outrf+= 1, inrf+= 4)
					*outrf= inrf[0]*0.35f + inrf[1]*0.45f + inrf[2]*0.2f;
			}
		}
		else if(type==CB_VEC2) {
			if(inbuf->type==CB_VAL) {
				for(; x>0; x--, outrf+= 2, inrf+= 1) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[0];
				}
			}
			else if(inbuf->type==CB_VEC3) {
				for(; x>0; x--, outrf+= 2, inrf+= 3) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[1];
				}
			}
			else if(inbuf->type==CB_RGBA) {
				for(; x>0; x--, outrf+= 2, inrf+= 4) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[1];
				}
			}
		}
		else if(type==CB_VEC3) {
			if(inbuf->type==CB_VAL) {
				for(; x>0; x--, outrf+= 3, inrf+= 1) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[0];
					outrf[2]= inrf[0];
				}
			}
			else if(inbuf->type==CB_VEC2) {
				for(; x>0; x--, outrf+= 3, inrf+= 2) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[1];
					outrf[2]= 0.0f;
				}
			}
			else if(inbuf->type==CB_RGBA) {
				for(; x>0; x--, outrf+= 3, inrf+= 4) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[1];
					outrf[2]= inrf[2];
				}
			}
		}
		else if(type==CB_RGBA) {
			if(inbuf->type==CB_VAL) {
				for(; x>0; x--, outrf+= 4, inrf+= 1) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[0];
					outrf[2]= inrf[0];
					outrf[3]= inrf[0];
				}
			}
			else if(inbuf->type==CB_VEC2) {
				for(; x>0; x--, outrf+= 4, inrf+= 2) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[1];
					outrf[2]= 0.0f;
					outrf[3]= 1.0f;
				}
			}
			else if(inbuf->type==CB_VEC3) {
				for(; x>0; x--, outrf+= 4, inrf+= 3) {
					outrf[0]= inrf[0];
					outrf[1]= inrf[1];
					outrf[2]= inrf[2];
					outrf[3]= 1.0f;
				}
			}
		}
		
		return outbuf;
	}
	return inbuf;
}

float *compbuf_get_pixel(CompBuf *cbuf, float *rectf, int x, int y, int xrad, int yrad)
{
	if(cbuf) {
		if(cbuf->rect_procedural) {
			cbuf->rect_procedural(cbuf, rectf, (float)x/(float)xrad, (float)y/(float)yrad);
			return rectf;
		}
		else {
			static float col[4]= {0.0f, 0.0f, 0.0f, 0.0f};
			
			/* map coords */
			x-= cbuf->xof;
			y-= cbuf->yof;
			
			if(y<-cbuf->yrad || y>= -cbuf->yrad+cbuf->y) return col;
			if(x<-cbuf->xrad || x>= -cbuf->xrad+cbuf->x) return col;
			
			return cbuf->rect + cbuf->type*( (cbuf->yrad+y)*cbuf->x + (cbuf->xrad+x) );
		}
	}
	else return rectf;
}

/* **************************************************** */

/* Pixel-to-Pixel operation, 1 Image in, 1 out */
static void composit1_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
									  void (*func)(bNode *, float *, float *), 
									  int src_type)
{
	CompBuf *src_use;
	float *outfp=out->rect, *srcfp;
	int xrad, yrad, x, y;
	
	src_use= typecheck_compbuf(src_buf, src_type);
	
	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			srcfp= compbuf_get_pixel(src_use, src_col, x, y, xrad, yrad);
			func(node, outfp, srcfp);
		}
	}
	
	if(src_use!=src_buf)
		free_compbuf(src_use);
}

/* Pixel-to-Pixel operation, 2 Images in, 1 out */
static void composit2_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
									  CompBuf *fac_buf, float *fac, void (*func)(bNode *, float *, float *, float *), 
									  int src_type, int fac_type)
{
	CompBuf *src_use, *fac_use;
	float *outfp=out->rect, *srcfp, *facfp;
	int xrad, yrad, x, y;
	
	src_use= typecheck_compbuf(src_buf, src_type);
	fac_use= typecheck_compbuf(fac_buf, fac_type);

	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			srcfp= compbuf_get_pixel(src_use, src_col, x, y, xrad, yrad);
			facfp= compbuf_get_pixel(fac_use, fac, x, y, xrad, yrad);
			
			func(node, outfp, srcfp, facfp);
		}
	}
	if(src_use!=src_buf)
		free_compbuf(src_use);
	if(fac_use!=fac_buf)
		free_compbuf(fac_use);
}

/* Pixel-to-Pixel operation, 3 Images in, 1 out */
static void composit3_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *src2_buf, float *src2_col, 
									  CompBuf *fac_buf, float *fac, void (*func)(bNode *, float *, float *, float *, float *), 
									  int src1_type, int src2_type, int fac_type)
{
	CompBuf *src1_use, *src2_use, *fac_use;
	float *outfp=out->rect, *src1fp, *src2fp, *facfp;
	int xrad, yrad, x, y;
	
	src1_use= typecheck_compbuf(src1_buf, src1_type);
	src2_use= typecheck_compbuf(src2_buf, src2_type);
	fac_use= typecheck_compbuf(fac_buf, fac_type);
	
	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			src1fp= compbuf_get_pixel(src1_use, src1_col, x, y, xrad, yrad);
			src2fp= compbuf_get_pixel(src2_use, src2_col, x, y, xrad, yrad);
			facfp= compbuf_get_pixel(fac_use, fac, x, y, xrad, yrad);
			
			func(node, outfp, src1fp, src2fp, facfp);
		}
	}
	
	if(src1_use!=src1_buf)
		free_compbuf(src1_use);
	if(src2_use!=src2_buf)
		free_compbuf(src2_use);
	if(fac_use!=fac_buf)
		free_compbuf(fac_use);
}

/* Pixel-to-Pixel operation, 4 Images in, 1 out */
static void composit4_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *fac1_buf, float *fac1, 
									  CompBuf *src2_buf, float *src2_col, CompBuf *fac2_buf, float *fac2, 
									  void (*func)(bNode *, float *, float *, float *, float *, float *), 
									  int src1_type, int fac1_type, int src2_type, int fac2_type)
{
	CompBuf *src1_use, *src2_use, *fac1_use, *fac2_use;
	float *outfp=out->rect, *src1fp, *src2fp, *fac1fp, *fac2fp;
	int xrad, yrad, x, y;
	
	src1_use= typecheck_compbuf(src1_buf, src1_type);
	src2_use= typecheck_compbuf(src2_buf, src2_type);
	fac1_use= typecheck_compbuf(fac1_buf, fac1_type);
	fac2_use= typecheck_compbuf(fac2_buf, fac2_type);
	
	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			src1fp= compbuf_get_pixel(src1_use, src1_col, x, y, xrad, yrad);
			src2fp= compbuf_get_pixel(src2_use, src2_col, x, y, xrad, yrad);
			fac1fp= compbuf_get_pixel(fac1_use, fac1, x, y, xrad, yrad);
			fac2fp= compbuf_get_pixel(fac2_use, fac2, x, y, xrad, yrad);
			
			func(node, outfp, src1fp, fac1fp, src2fp, fac2fp);
		}
	}
	
	if(src1_use!=src1_buf)
		free_compbuf(src1_use);
	if(src2_use!=src2_buf)
		free_compbuf(src2_use);
	if(fac1_use!=fac1_buf)
		free_compbuf(fac1_use);
	if(fac2_use!=fac2_buf)
		free_compbuf(fac2_use);
}


static CompBuf *valbuf_from_rgbabuf(CompBuf *cbuf, int channel)
{
	CompBuf *valbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
	float *valf, *rectf;
	int tot;
	
	/* warning note: xof and yof are applied in pixelprocessor, but should be copied otherwise? */
	valbuf->xof= cbuf->xof;
	valbuf->yof= cbuf->yof;
	
	valf= valbuf->rect;
	
	/* defaults to returning alpha channel */
	if ((channel < CHAN_R) && (channel > CHAN_A)) channel = CHAN_A;

	rectf= cbuf->rect + channel;
	
	for(tot= cbuf->x*cbuf->y; tot>0; tot--, valf++, rectf+=4)
		*valf= *rectf;
	
	return valbuf;
}

static void generate_preview(bNode *node, CompBuf *stackbuf)
{
	bNodePreview *preview= node->preview;
	
	if(preview && stackbuf) {
		CompBuf *cbuf, *stackbuf_use;
		
		if(stackbuf->rect==NULL) return;
		
		stackbuf_use= typecheck_compbuf(stackbuf, CB_RGBA);
		
		if(stackbuf->x > stackbuf->y) {
			preview->xsize= 140;
			preview->ysize= (140*stackbuf->y)/stackbuf->x;
		}
		else {
			preview->ysize= 140;
			preview->xsize= (140*stackbuf->x)/stackbuf->y;
		}
		
		cbuf= scalefast_compbuf(stackbuf_use, preview->xsize, preview->ysize);
		
		/* this ensures free-compbuf does the right stuff */
		SWAP(float *, cbuf->rect, node->preview->rect);
		
		free_compbuf(cbuf);
		if(stackbuf_use!=stackbuf)
			free_compbuf(stackbuf_use);

	}
}

/* ******************************************************** */
/* ********* Composit Node type definitions ***************** */
/* ******************************************************** */

/* SocketType syntax: 
   socket type, max connections (0 is no limit), name, 4 values for default, 2 values for range */

/* Verification rule: If name changes, a saved socket and its links will be removed! Type changes are OK */

/* **************** VIEWER ******************** */
static bNodeSocketType cmp_node_viewer_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_copy_rgba(bNode *node, float *out, float *in)
{
	QUATCOPY(out, in);
}
static void do_copy_rgb(bNode *node, float *out, float *in)
{
	VECCOPY(out, in);
	out[3]= 1.0f;
}
static void do_copy_value(bNode *node, float *out, float *in)
{
	out[0]= in[0];
}
static void do_copy_a_rgba(bNode *node, float *out, float *in, float *fac)
{
	VECCOPY(out, in);
	out[3]= *fac;
}

static void node_composit_exec_viewer(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha, z */
	
	if(node->id && (node->flag & NODE_DO_OUTPUT)) {	/* only one works on out */
		RenderData *rd= data;
		Image *ima= (Image *)node->id;
		ImBuf *ibuf;
		CompBuf *cbuf, *tbuf;
		int rectx, recty;
		
		BKE_image_user_calc_imanr(node->storage, rd->cfra, 0);

		/* always returns for viewer image, but we check nevertheless */
		ibuf= BKE_image_get_ibuf(ima, node->storage);
		if(ibuf==NULL) {
			printf("node_composit_exec_viewer error\n");
			return;
		}
		
		/* free all in ibuf */
		imb_freerectImBuf(ibuf);
		imb_freerectfloatImBuf(ibuf);
		IMB_freezbuffloatImBuf(ibuf);
		
		/* get size */
		tbuf= in[0]->data?in[0]->data:(in[1]->data?in[1]->data:in[2]->data);
		if(tbuf==NULL) {
			rectx= 320; recty= 256;
		}
		else {
			rectx= tbuf->x;
			recty= tbuf->y;
		}
		
		/* make ibuf, and connect to ima */
		ibuf->x= rectx;
		ibuf->y= recty;
		imb_addrectfloatImBuf(ibuf);
		
		ima->ok= IMA_OK_LOADED;

		/* now we combine the input with ibuf */
		cbuf= alloc_compbuf(rectx, recty, CB_RGBA, 0);	/* no alloc*/
		cbuf->rect= ibuf->rect_float;
		
		/* when no alpha, we can simply copy */
		if(in[1]->data==NULL) {
			composit1_pixel_processor(node, cbuf, in[0]->data, in[0]->vec, do_copy_rgba, CB_RGBA);
		}
		else
			composit2_pixel_processor(node, cbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
		
		/* zbuf option */
		if(in[2]->data) {
			CompBuf *zbuf= alloc_compbuf(rectx, recty, CB_VAL, 1);
			ibuf->zbuf_float= zbuf->rect;
			ibuf->mall |= IB_zbuffloat;
			
			composit1_pixel_processor(node, zbuf, in[2]->data, in[2]->vec, do_copy_value, CB_VAL);
			
			/* free compbuf, but not the rect */
			zbuf->malloc= 0;
			free_compbuf(zbuf);
		}

		generate_preview(node, cbuf);
		free_compbuf(cbuf);

	}
	else if(in[0]->data) {
		generate_preview(node, in[0]->data);
	}
}

static bNodeType cmp_node_viewer= {
	/* type code   */	CMP_NODE_VIEWER,
	/* name        */	"Viewer",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	cmp_node_viewer_in,
	/* output sock */	NULL,
	/* storage     */	"ImageUser",
	/* execfunc    */	node_composit_exec_viewer
	
};

/* **************** SPLIT VIEWER ******************** */
static bNodeSocketType cmp_node_splitviewer_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_copy_split_rgba(bNode *node, float *out, float *in1, float *in2, float *fac)
{
	if(*fac==0.0f) {
		QUATCOPY(out, in1);
	}
	else {
		QUATCOPY(out, in2);
	}
}

static void node_composit_exec_splitviewer(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: image image */
	
	if(in[0]->data==NULL || in[1]->data==NULL)
		return;
	
	if(node->id && (node->flag & NODE_DO_OUTPUT)) {	/* only one works on out */
		Image *ima= (Image *)node->id;
		RenderData *rd= data;
		ImBuf *ibuf;
		CompBuf *cbuf, *buf1, *buf2, *mask;
		int x, y;
		float offset;
		
		buf1= typecheck_compbuf(in[0]->data, CB_RGBA);
		buf2= typecheck_compbuf(in[1]->data, CB_RGBA);
		
		BKE_image_user_calc_imanr(node->storage, rd->cfra, 0);
		
		/* always returns for viewer image, but we check nevertheless */
		ibuf= BKE_image_get_ibuf(ima, node->storage);
		if(ibuf==NULL) {
			printf("node_composit_exec_viewer error\n");
			return;
		}
		
		/* free all in ibuf */
		imb_freerectImBuf(ibuf);
		imb_freerectfloatImBuf(ibuf);
		IMB_freezbuffloatImBuf(ibuf);
		
		/* make ibuf, and connect to ima */
		ibuf->x= buf1->x;
		ibuf->y= buf1->y;
		imb_addrectfloatImBuf(ibuf);
		
		ima->ok= IMA_OK_LOADED;

		/* output buf */
		cbuf= alloc_compbuf(buf1->x, buf1->y, CB_RGBA, 0);	/* no alloc*/
		cbuf->rect= ibuf->rect_float;
		
		/* mask buf */
		mask= alloc_compbuf(buf1->x, buf1->y, CB_VAL, 1);
		
		
		/* Check which offset mode is selected and limit offset if needed */
		if(node->custom2 == 0) {
			offset = buf1->x / 100.0f * node->custom1;
			CLAMP(offset, 0, buf1->x);
		}
		else {
			offset = buf1->y / 100.0f * node->custom1;
			CLAMP(offset, 0, buf1->y);
		}
		
		if(node->custom2 == 0) {
			for(y=0; y<buf1->y; y++) {
				float *fac= mask->rect + y*buf1->x;
				for(x=offset; x>0; x--, fac++)
					*fac= 1.0f;
			}
		}
		else {
			for(y=0; y<offset; y++) {
				float *fac= mask->rect + y*buf1->x;
				for(x=buf1->x; x>0; x--, fac++)
					*fac= 1.0f;
			}
		}
		
		composit3_pixel_processor(node, cbuf, buf1, in[0]->vec, buf2, in[1]->vec, mask, NULL, do_copy_split_rgba, CB_RGBA, CB_RGBA, CB_VAL);
		
		generate_preview(node, cbuf);
		free_compbuf(cbuf);
		free_compbuf(mask);
		
		if(in[0]->data != buf1) 
			free_compbuf(buf1);
		if(in[1]->data != buf2) 
			free_compbuf(buf2);
	}
}

static bNodeType cmp_node_splitviewer= {
	/* type code   */	CMP_NODE_SPLITVIEWER,
	/* name        */	"SplitViewer",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_splitviewer_in,
	/* output sock */	NULL,
	/* storage     */	"ImageUser",
	/* execfunc    */	node_composit_exec_splitviewer
	
};


/* **************** COMPOSITE ******************** */
static bNodeSocketType cmp_node_composite_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* applies to render pipeline */
static void node_composit_exec_composite(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha, z */
	
	if(node->flag & NODE_DO_OUTPUT) {	/* only one works on out */
		RenderData *rd= data;
		if(rd->scemode & R_DOCOMP) {
			RenderResult *rr= RE_GetResult(RE_GetRender(G.scene->id.name)); /* G.scene is WEAK! */
			if(rr) {
				CompBuf *outbuf, *zbuf=NULL;
				
				if(rr->rectf) 
					MEM_freeN(rr->rectf);
				outbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 1);
				
				if(in[1]->data==NULL)
					composit1_pixel_processor(node, outbuf, in[0]->data, in[0]->vec, do_copy_rgba, CB_RGBA);
				else
					composit2_pixel_processor(node, outbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
				
				if(in[2]->data) {
					if(rr->rectz) 
						MEM_freeN(rr->rectz);
					zbuf= alloc_compbuf(rr->rectx, rr->recty, CB_VAL, 1);
					composit1_pixel_processor(node, zbuf, in[2]->data, in[2]->vec, do_copy_value, CB_VAL);
					rr->rectz= zbuf->rect;
					zbuf->malloc= 0;
					free_compbuf(zbuf);
				}
				generate_preview(node, outbuf);
				
				/* we give outbuf to rr... */
				rr->rectf= outbuf->rect;
				outbuf->malloc= 0;
				free_compbuf(outbuf);
				
				/* signal for imageviewer to refresh (it converts to byte rects...) */
				BKE_image_signal(BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result"), NULL, IMA_SIGNAL_FREE);
				return;
			}
		}
	}
	if(in[0]->data)
		generate_preview(node, in[0]->data);
}

static bNodeType cmp_node_composite= {
	/* type code   */	CMP_NODE_COMPOSITE,
	/* name        */	"Composite",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	cmp_node_composite_in,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_composite
	
};

/* **************** OUTPUT FILE ******************** */
static bNodeSocketType cmp_node_output_file_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_output_file(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	
	if(in[0]->data) {
		RenderData *rd= data;
		NodeImageFile *nif= node->storage;
		if(nif->sfra!=nif->efra && (rd->cfra<nif->sfra || rd->cfra>nif->efra)) {
			return;	/* BAIL OUT RETURN */
		}
		else {
			CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
			ImBuf *ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
			char string[256];
			
			ibuf->rect_float= cbuf->rect;
			ibuf->dither= rd->dither_intensity;
			if(in[1]->data) {
				CompBuf *zbuf= in[1]->data;
				if(zbuf->type==CB_VAL && zbuf->x==cbuf->x && zbuf->y==cbuf->y) {
					nif->subimtype|= R_OPENEXR_ZBUF;
					ibuf->zbuf_float= zbuf->rect;
				}
			}
			
			BKE_makepicstring(string, nif->name, rd->cfra, nif->imtype);
			
			if(0 == BKE_write_ibuf(ibuf, string, nif->imtype, nif->subimtype, nif->imtype==R_OPENEXR?nif->codec:nif->quality))
				printf("Cannot save Node File Output to %s\n", string);
			else
				printf("Saved: %s\n", string);
			
			IMB_freeImBuf(ibuf);	
			
			generate_preview(node, cbuf);
			
			if(in[0]->data != cbuf) 
				free_compbuf(cbuf);
		}
	}
}

static bNodeType cmp_node_output_file= {
	/* type code   */	CMP_NODE_OUTPUT_FILE,
	/* name        */	"File Output",
	/* width+range */	140, 80, 300,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	cmp_node_output_file_in,
	/* output sock */	NULL,
	/* storage     */	"NodeImageFile",
	/* execfunc    */	node_composit_exec_output_file
	
};

/* **************** IMAGE (and RenderResult, multilayer image) ******************** */

/* output socket defines */
#define RRES_OUT_IMAGE	0
#define RRES_OUT_ALPHA	1
#define RRES_OUT_Z		2
#define RRES_OUT_NORMAL	3
#define RRES_OUT_UV		4
#define RRES_OUT_VEC	5
#define RRES_OUT_RGBA	6
#define RRES_OUT_DIFF	7
#define RRES_OUT_SPEC	8
#define RRES_OUT_SHADOW	9
#define RRES_OUT_AO		10
#define RRES_OUT_REFLECT 11
#define RRES_OUT_REFRACT 12
#define RRES_OUT_RADIO	 13
#define RRES_OUT_INDEXOB 14

static bNodeSocketType cmp_node_rlayers_out[]= {
	{	SOCK_RGBA, 0, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "UV",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Speed",	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Specular",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Shadow",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "AO",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Reflect",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Refract",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Radio",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "IndexOB",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


/* note: this function is used for multilayer too, to ensure uniform 
   handling with BKE_image_get_ibuf() */
static CompBuf *node_composit_get_image(RenderData *rd, Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf;
	CompBuf *stackbuf;
	int type;
	
	ibuf= BKE_image_get_ibuf(ima, iuser);
	if(ibuf==NULL)
		return NULL;
	
	if(ibuf->rect_float==NULL)
		IMB_float_from_rect(ibuf);
	
	type= ibuf->channels;
	
	if(rd->scemode & R_COMP_CROP) {
		stackbuf= get_cropped_compbuf(&rd->disprect, ibuf->rect_float, ibuf->x, ibuf->y, type);
	}
	else {
		/* we put imbuf copy on stack, cbuf knows rect is from other ibuf when freed! */
		stackbuf= alloc_compbuf(ibuf->x, ibuf->y, type, 0);
		stackbuf->rect= ibuf->rect_float;
	}
	
	return stackbuf;
}

static CompBuf *node_composit_get_zimage(bNode *node, RenderData *rd)
{
	ImBuf *ibuf= BKE_image_get_ibuf((Image *)node->id, node->storage);
	CompBuf *zbuf= NULL;
	
	if(ibuf && ibuf->zbuf_float) {
		if(rd->scemode & R_COMP_CROP) {
			zbuf= get_cropped_compbuf(&rd->disprect, ibuf->zbuf_float, ibuf->x, ibuf->y, CB_VAL);
		}
		else {
			zbuf= alloc_compbuf(ibuf->x, ibuf->y, CB_VAL, 0);
			zbuf->rect= ibuf->zbuf_float;
		}
	}
	return zbuf;
}

/* check if layer is available, returns pass buffer */
static CompBuf *compbuf_multilayer_get(RenderData *rd, RenderLayer *rl, Image *ima, ImageUser *iuser, int passtype)
{
	RenderPass *rpass;
	short index;
	
	for(index=0, rpass= rl->passes.first; rpass; rpass= rpass->next, index++)
		if(rpass->passtype==passtype)
			break;
	
	if(rpass) {
		CompBuf *cbuf;
		
		iuser->pass= index;
		BKE_image_multilayer_index(ima->rr, iuser);
		cbuf= node_composit_get_image(rd, ima, iuser);
		
		return cbuf;
	}
	return NULL;
}

void outputs_multilayer_get(RenderData *rd, RenderLayer *rl, bNodeStack **out, Image *ima, ImageUser *iuser)
{
	if(out[RRES_OUT_Z]->hasoutput)
		out[RRES_OUT_Z]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_Z);
	if(out[RRES_OUT_VEC]->hasoutput)
		out[RRES_OUT_VEC]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_VECTOR);
	if(out[RRES_OUT_NORMAL]->hasoutput)
		out[RRES_OUT_NORMAL]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_NORMAL);
	if(out[RRES_OUT_UV]->hasoutput)
		out[RRES_OUT_UV]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_UV);
	
	if(out[RRES_OUT_RGBA]->hasoutput)
		out[RRES_OUT_RGBA]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_RGBA);
	if(out[RRES_OUT_DIFF]->hasoutput)
		out[RRES_OUT_DIFF]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_DIFFUSE);
	if(out[RRES_OUT_SPEC]->hasoutput)
		out[RRES_OUT_SPEC]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_SPEC);
	if(out[RRES_OUT_SHADOW]->hasoutput)
		out[RRES_OUT_SHADOW]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_SHADOW);
	if(out[RRES_OUT_AO]->hasoutput)
		out[RRES_OUT_AO]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_AO);
	if(out[RRES_OUT_REFLECT]->hasoutput)
		out[RRES_OUT_REFLECT]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_REFLECT);
	if(out[RRES_OUT_REFRACT]->hasoutput)
		out[RRES_OUT_REFRACT]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_REFRACT);
	if(out[RRES_OUT_RADIO]->hasoutput)
		out[RRES_OUT_RADIO]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_RADIO);
	if(out[RRES_OUT_INDEXOB]->hasoutput)
		out[RRES_OUT_INDEXOB]->data= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_INDEXOB);
	
}


static void node_composit_exec_image(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	if(node->id) {
		RenderData *rd= data;
		Image *ima= (Image *)node->id;
		ImageUser *iuser= (ImageUser *)node->storage;
		CompBuf *stackbuf= NULL;
		
		/* first set the right frame number in iuser */
		BKE_image_user_calc_imanr(iuser, rd->cfra, 0);
		
		/* force a load, we assume iuser index will be set OK anyway */
		if(ima->type==IMA_TYPE_MULTILAYER)
			BKE_image_get_ibuf(ima, iuser);
		
		if(ima->type==IMA_TYPE_MULTILAYER && ima->rr) {
			RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
			
			if(rl) {
				out[0]->data= stackbuf= compbuf_multilayer_get(rd, rl, ima, iuser, SCE_PASS_COMBINED);
				
				/* go over all layers */
				outputs_multilayer_get(rd, rl, out, ima, iuser);
			}
		}
		else {
			stackbuf= node_composit_get_image(rd, ima, iuser);

			/* put image on stack */	
			out[0]->data= stackbuf;
			
			if(out[2]->hasoutput)
				out[2]->data= node_composit_get_zimage(node, rd);
		}
		
		/* alpha and preview for both types */
		if(stackbuf) {
			if(out[1]->hasoutput)
				out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

			generate_preview(node, stackbuf);
		}
	}	
}

/* uses node->storage to indicate animated image */

static bNodeType cmp_node_image= {
	/* type code   */	CMP_NODE_IMAGE,
	/* name        */	"Image",
	/* width+range */	120, 80, 300,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rlayers_out,
	/* storage     */	"ImageUser",
	/* execfunc    */	node_composit_exec_image
	
};

/* **************** RENDER RESULT ******************** */

static CompBuf *compbuf_from_pass(RenderData *rd, RenderLayer *rl, int rectx, int recty, int passcode)
{
	float *fp= RE_RenderLayerGetPass(rl, passcode);
	if(fp) {
		CompBuf *buf;
		int buftype= CB_VEC3;
		
		if(ELEM(passcode, SCE_PASS_Z, SCE_PASS_INDEXOB))
			buftype= CB_VAL;
		else if(passcode==SCE_PASS_VECTOR)
			buftype= CB_VEC4;
		else if(ELEM(passcode, SCE_PASS_COMBINED, SCE_PASS_RGBA))
			buftype= CB_RGBA;
		
		if(rd->scemode & R_COMP_CROP)
			buf= get_cropped_compbuf(&rd->disprect, fp, rectx, recty, buftype);
		else {
			buf= alloc_compbuf(rectx, recty, buftype, 0);
			buf->rect= fp;
		}
		return buf;
	}
	return NULL;
}

void node_composit_rlayers_out(RenderData *rd, RenderLayer *rl, bNodeStack **out, int rectx, int recty)
{
	if(out[RRES_OUT_Z]->hasoutput)
		out[RRES_OUT_Z]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_Z);
	if(out[RRES_OUT_VEC]->hasoutput)
		out[RRES_OUT_VEC]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_VECTOR);
	if(out[RRES_OUT_NORMAL]->hasoutput)
		out[RRES_OUT_NORMAL]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_NORMAL);
	if(out[RRES_OUT_UV]->hasoutput)
		out[RRES_OUT_UV]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_UV);
	
	if(out[RRES_OUT_RGBA]->hasoutput)
		out[RRES_OUT_RGBA]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_RGBA);
	if(out[RRES_OUT_DIFF]->hasoutput)
		out[RRES_OUT_DIFF]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_DIFFUSE);
	if(out[RRES_OUT_SPEC]->hasoutput)
		out[RRES_OUT_SPEC]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_SPEC);
	if(out[RRES_OUT_SHADOW]->hasoutput)
		out[RRES_OUT_SHADOW]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_SHADOW);
	if(out[RRES_OUT_AO]->hasoutput)
		out[RRES_OUT_AO]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_AO);
	if(out[RRES_OUT_REFLECT]->hasoutput)
		out[RRES_OUT_REFLECT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_REFLECT);
	if(out[RRES_OUT_REFRACT]->hasoutput)
		out[RRES_OUT_REFRACT]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_REFRACT);
	if(out[RRES_OUT_RADIO]->hasoutput)
		out[RRES_OUT_RADIO]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_RADIO);
	if(out[RRES_OUT_INDEXOB]->hasoutput)
		out[RRES_OUT_INDEXOB]->data= compbuf_from_pass(rd, rl, rectx, recty, SCE_PASS_INDEXOB);
	
}

static void node_composit_exec_rlayers(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	Scene *sce= node->id?(Scene *)node->id:G.scene; /* G.scene is WEAK! */
	RenderData *rd= data;
	RenderResult *rr;
	
	rr= RE_GetResult(RE_GetRender(sce->id.name));
		
	if(rr) {
		SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
		if(srl) {
			RenderLayer *rl= RE_GetRenderLayer(rr, srl->name);
			if(rl && rl->rectf) {
				CompBuf *stackbuf;
				
				/* we put render rect on stack, cbuf knows rect is from other ibuf when freed! */
				if(rd->scemode & R_COMP_CROP)
					stackbuf= get_cropped_compbuf(&rd->disprect, rl->rectf, rr->rectx, rr->recty, CB_RGBA);
				else {
					stackbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 0);
					stackbuf->rect= rl->rectf;
				}
				if(stackbuf==NULL) {
					printf("Error; Preview Panel in UV Window returns zero sized image\n");
				}
				else {
					stackbuf->xof= rr->xof;
					stackbuf->yof= rr->yof;
					
					/* put on stack */	
					out[RRES_OUT_IMAGE]->data= stackbuf;
					
					if(out[RRES_OUT_ALPHA]->hasoutput)
						out[RRES_OUT_ALPHA]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);
					
					node_composit_rlayers_out(rd, rl, out, rr->rectx, rr->recty);

					generate_preview(node, stackbuf);
				}
			}
		}
	}	
}

/* custom1 = render layer in use */
/* custom2 = re-render tag */
static bNodeType cmp_node_rlayers= {
	/* type code   */	CMP_NODE_R_LAYERS,
	/* name        */	"Render Layers",
	/* width+range */	150, 100, 300,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rlayers_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rlayers
	
};

/* **************** TEXTURE ******************** */
static bNodeSocketType cmp_node_texture_in[]= {
	{	SOCK_VECTOR, 0, "Offset",		0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 2.0f},
	{	SOCK_VECTOR, 0, "Scale",		1.0f, 1.0f, 1.0f, 1.0f, -10.0f, 10.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_texture_out[]= {
	{	SOCK_VALUE, 0, "Value",		1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA , 0, "Color",		1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* called without rect allocated */
static void texture_procedural(CompBuf *cbuf, float *col, float xco, float yco)
{
	bNode *node= cbuf->node;
	bNodeSocket *sock= node->inputs.first;
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float vec[3], *size, nor[3]={0.0f, 0.0f, 0.0f};
	int retval, type= cbuf->type;
	
	size= sock->next->ns.vec;
	
	vec[0]= size[0]*(xco + sock->ns.vec[0]);
	vec[1]= size[1]*(yco + sock->ns.vec[1]);
	vec[2]= size[2]*sock->ns.vec[2];
	
	retval= multitex_ext((Tex *)node->id, vec, NULL, NULL, 0, &texres);
	
	if(type==CB_VAL) {
		if(texres.talpha)
			col[0]= texres.ta;
		else
			col[0]= texres.tin;
	}
	else if(type==CB_RGBA) {
		if(texres.talpha)
			col[3]= texres.ta;
		else
			col[3]= texres.tin;
		
		if((retval & TEX_RGB)) {
			col[0]= texres.tr;
			col[1]= texres.tg;
			col[2]= texres.tb;
		}
		else col[0]= col[1]= col[2]= col[3];
	}
	else { 
		VECCOPY(col, nor);
	}
}

/* texture node outputs get a small rect, to make sure all other nodes accept it */
/* only the pixel-processor nodes do something with it though */
static void node_composit_exec_texture(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* outputs: value, color, normal */
	
	if(node->id) {
		/* first make the preview image */
		CompBuf *prevbuf= alloc_compbuf(140, 140, CB_RGBA, 1); // alloc
		
		prevbuf->rect_procedural= texture_procedural;
		prevbuf->node= node;
		composit1_pixel_processor(node, prevbuf, prevbuf, out[0]->vec, do_copy_rgba, CB_RGBA);
		generate_preview(node, prevbuf);
		free_compbuf(prevbuf);
		
		if(out[0]->hasoutput) {
			CompBuf *stackbuf= alloc_compbuf(140, 140, CB_VAL, 1); // alloc
			
			stackbuf->rect_procedural= texture_procedural;
			stackbuf->node= node;
			
			out[0]->data= stackbuf;
		}
		if(out[1]->hasoutput) {
			CompBuf *stackbuf= alloc_compbuf(140, 140, CB_RGBA, 1); // alloc
			
			stackbuf->rect_procedural= texture_procedural;
			stackbuf->node= node;
			
			out[1]->data= stackbuf;
		}
	}
}

static bNodeType cmp_node_texture= {
	/* type code   */	CMP_NODE_TEXTURE,
	/* name        */	"Texture",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	cmp_node_texture_in,
	/* output sock */	cmp_node_texture_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_texture
	
};

/* **************** NORMAL  ******************** */
static bNodeSocketType cmp_node_normal_in[]= {
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_normal_out[]= {
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_VALUE, 0, "Dot",		1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_normal(bNode *node, float *out, float *in)
{
	bNodeSocket *sock= node->outputs.first;
	float *nor= sock->ns.vec;
	
	/* render normals point inside... the widget points outside */
	out[0]= -INPR(nor, in);
}

/* generates normal, does dot product */
static void node_composit_exec_normal(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	/* stack order input:  normal */
	/* stack order output: normal, value */
	
	/* input no image? then only vector op */
	if(in[0]->data==NULL) {
		VECCOPY(out[0]->vec, sock->ns.vec);
		/* render normals point inside... the widget points outside */
		out[1]->vec[0]= -INPR(out[0]->vec, in[0]->vec);
	}
	else if(out[1]->hasoutput) {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_normal, CB_VEC3);
		
		out[1]->data= stackbuf;
	}
	
	
}

static bNodeType cmp_node_normal= {
	/* type code   */	CMP_NODE_NORMAL,
	/* name        */	"Normal",
	/* width+range */	100, 60, 200,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_normal_in,
	/* output sock */	cmp_node_normal_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_normal
	
};

/* **************** CURVE Time  ******************** */

/* custom1 = sfra, custom2 = efra */
static bNodeSocketType cmp_node_time_out[]= {
	{	SOCK_VALUE, 0, "Fac",	1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_time(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order output: fac */
	float fac= 0.0f;
	
	if(node->custom1 < node->custom2)
		fac= (G.scene->r.cfra - node->custom1)/(float)(node->custom2-node->custom1);
	
	out[0]->vec[0]= curvemapping_evaluateF(node->storage, 0, fac);
}

static bNodeType cmp_node_time= {
	/* type code   */	CMP_NODE_TIME,
	/* name        */	"Time",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_time_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_time
};

/* **************** CURVE VEC  ******************** */
static bNodeSocketType cmp_node_curve_vec_in[]= {
	{	SOCK_VECTOR, 1, "Vector",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_curve_vec_out[]= {
	{	SOCK_VECTOR, 0, "Vector",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_curve_vec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  vec */
	/* stack order output: vec */
	
	curvemapping_evaluate_premulRGBF(node->storage, out[0]->vec, in[0]->vec);
}

static bNodeType cmp_node_curve_vec= {
	/* type code   */	CMP_NODE_CURVE_VEC,
	/* name        */	"Vector Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_curve_vec_in,
	/* output sock */	cmp_node_curve_vec_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_curve_vec
	
};

/* **************** CURVE RGB  ******************** */
static bNodeSocketType cmp_node_curve_rgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",	1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_curve_rgb_out[]= {
	{	SOCK_RGBA, 0, "Image",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_curves(bNode *node, float *out, float *in)
{
	curvemapping_evaluate_premulRGBF(node->storage, out, in);
	out[3]= in[3];
}

static void do_curves_fac(bNode *node, float *out, float *in, float *fac)
{
	
	if(*fac>=1.0)
		curvemapping_evaluate_premulRGBF(node->storage, out, in);
	else if(*fac<=0.0) {
		VECCOPY(out, in);
	}
	else {
		float col[4], mfac= 1.0f-*fac;
		curvemapping_evaluate_premulRGBF(node->storage, col, in);
		out[0]= mfac*in[0] + *fac*col[0];
		out[1]= mfac*in[1] + *fac*col[1];
		out[2]= mfac*in[2] + *fac*col[2];
	}
	out[3]= in[3];
}

static void node_composit_exec_curve_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  fac, image */
	/* stack order output: image */
	
	if(out[0]->hasoutput==0)
		return;

	/* input no image? then only color operation */
	if(in[1]->data==NULL) {
		curvemapping_evaluateRGBF(node->storage, out[0]->vec, in[1]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		if(in[0]->data)
			composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_curves_fac, CB_RGBA, CB_VAL);
		else
			composit1_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, do_curves, CB_RGBA);
		
		out[0]->data= stackbuf;
	}
	
}

static bNodeType cmp_node_curve_rgb= {
	/* type code   */	CMP_NODE_CURVE_RGB,
	/* name        */	"RGB Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_curve_rgb_in,
	/* output sock */	cmp_node_curve_rgb_out,
	/* storage     */	"CurveMapping",
	/* execfunc    */	node_composit_exec_curve_rgb
	
};

/* **************** VALUE ******************** */
static bNodeSocketType cmp_node_value_out[]= {
	{	SOCK_VALUE, 0, "Value",		0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_value(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	out[0]->vec[0]= sock->ns.vec[0];
}

static bNodeType cmp_node_value= {
	/* type code   */	CMP_NODE_VALUE,
	/* name        */	"Value",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_value_out,
	/* storage     */	"", 
	/* execfunc    */	node_composit_exec_value
	
};

/* **************** RGB ******************** */
static bNodeSocketType cmp_node_rgb_out[]= {
	{	SOCK_RGBA, 0, "RGBA",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	VECCOPY(out[0]->vec, sock->ns.vec);
}

static bNodeType cmp_node_rgb= {
	/* type code   */	CMP_NODE_RGB,
	/* name        */	"RGB",
	/* width+range */	100, 60, 140,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rgb_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rgb
	
};

/* **************** Hue Saturation ******************** */
static bNodeSocketType cmp_node_hue_sat_in[]= {
	{	SOCK_VALUE, 1, "Fac",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_hue_sat_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_hue_sat_fac(bNode *node, float *out, float *in, float *fac)
{
	NodeHueSat *nhs= node->storage;
	
	if(*fac!=0.0f && (nhs->hue!=0.5f || nhs->sat!=1.0 || nhs->val!=1.0)) {
		float col[3], hsv[3], mfac= 1.0f - *fac;
		
		rgb_to_hsv(in[0], in[1], in[2], hsv, hsv+1, hsv+2);
		hsv[0]+= (nhs->hue - 0.5f);
		if(hsv[0]>1.0) hsv[0]-=1.0; else if(hsv[0]<0.0) hsv[0]+= 1.0;
		hsv[1]*= nhs->sat;
		if(hsv[1]>1.0) hsv[1]= 1.0; else if(hsv[1]<0.0) hsv[1]= 0.0;
		hsv[2]*= nhs->val;
		if(hsv[2]>1.0) hsv[2]= 1.0; else if(hsv[2]<0.0) hsv[2]= 0.0;
		hsv_to_rgb(hsv[0], hsv[1], hsv[2], col, col+1, col+2);
		
		out[0]= mfac*in[0] + *fac*col[0];
		out[1]= mfac*in[1] + *fac*col[1];
		out[2]= mfac*in[2] + *fac*col[2];
		out[3]= in[3];
	}
	else {
		QUATCOPY(out, in);
	}
}

static void node_composit_exec_hue_sat(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: Fac, Image */
	/* stack order out: Image */
	if(out[0]->hasoutput==0) return;
	
	/* input no image? then only color operation */
	if(in[1]->data==NULL) {
		do_hue_sat_fac(node, out[0]->vec, in[1]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit2_pixel_processor(node, stackbuf, cbuf, in[1]->vec, in[0]->data, in[0]->vec, do_hue_sat_fac, CB_RGBA, CB_VAL);

		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_hue_sat= {
	/* type code   */	CMP_NODE_HUE_SAT,
	/* name        */	"Hue Saturation Value",
	/* width+range */	150, 80, 250,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_hue_sat_in,
	/* output sock */	cmp_node_hue_sat_out,
	/* storage     */	"NodeHueSat", 
	/* execfunc    */	node_composit_exec_hue_sat
	
};

/* **************** MIX RGB ******************** */
static bNodeSocketType cmp_node_mix_rgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_mix_rgb_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_mix_rgb(bNode *node, float *out, float *in1, float *in2, float *fac)
{
	float col[3];
	
	VECCOPY(col, in1);
	if(node->custom2)
		ramp_blend(node->custom1, col, col+1, col+2, in2[3]*fac[0], in2);
	else
		ramp_blend(node->custom1, col, col+1, col+2, fac[0], in2);
	VECCOPY(out, col);
	out[3]= in1[3];
}

static void node_composit_exec_mix_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, Image, Image */
	/* stack order out: Image */
	float *fac= in[0]->vec;
	
	if(out[0]->hasoutput==0) return;
	
	/* input no image? then only color operation */
	if(in[1]->data==NULL && in[2]->data==NULL) {
		do_mix_rgb(node, out[0]->vec, in[1]->vec, in[2]->vec, fac);
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf= in[1]->data?in[1]->data:in[2]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, fac, do_mix_rgb, CB_RGBA, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}

/* custom1 = mix type */
static bNodeType cmp_node_mix_rgb= {
	/* type code   */	CMP_NODE_MIX_RGB,
	/* name        */	"Mix",
	/* width+range */	80, 60, 120,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_mix_rgb_in,
	/* output sock */	cmp_node_mix_rgb_out,
	/* storage     */	"", 
	/* execfunc    */	node_composit_exec_mix_rgb
	
};

/* **************** FILTER  ******************** */
static bNodeSocketType cmp_node_filter_in[]= {
	{	SOCK_VALUE, 1, "Fac",			1.0f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_filter_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_filter_edge(CompBuf *out, CompBuf *in, float *filter, float fac)
{
	float *row1, *row2, *row3;
	float *fp, f1, f2, mfac= 1.0f-fac;
	int rowlen, x, y, c, pix= in->type;
	
	rowlen= in->x;
	
	for(y=0; y<in->y; y++) {
		/* setup rows */
		if(y==0) row1= in->rect;
		else row1= in->rect + pix*(y-1)*rowlen;
		
		row2= in->rect + y*pix*rowlen;
		
		if(y==in->y-1) row3= row2;
		else row3= row2 + pix*rowlen;
		
		fp= out->rect + pix*y*rowlen;
		
		if(pix==CB_RGBA) {
			QUATCOPY(fp, row2);
			fp+= pix;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<3; c++) {
					f1= filter[0]*row1[0] + filter[1]*row1[4] + filter[2]*row1[8] + filter[3]*row2[0] + filter[4]*row2[4] + filter[5]*row2[8] + filter[6]*row3[0] + filter[7]*row3[4] + filter[8]*row3[8];
					f2= filter[0]*row1[0] + filter[3]*row1[4] + filter[6]*row1[8] + filter[1]*row2[0] + filter[4]*row2[4] + filter[7]*row2[8] + filter[2]*row3[0] + filter[5]*row3[4] + filter[8]*row3[8];
					fp[0]= mfac*row2[4] + fac*sqrt(f1*f1 + f2*f2);
					fp++; row1++; row2++; row3++;
				}
				fp[0]= row2[4];
				/* no alpha... will clear it completely */
				fp++; row1++; row2++; row3++;
			}
			QUATCOPY(fp, row2+4);
		}
		else if(pix==CB_VAL) {
			for(x=2; x<rowlen; x++) {
				f1= filter[0]*row1[0] + filter[1]*row1[1] + filter[2]*row1[2] + filter[3]*row2[0] + filter[4]*row2[1] + filter[5]*row2[2] + filter[6]*row3[0] + filter[7]*row3[1] + filter[8]*row3[2];
				f2= filter[0]*row1[0] + filter[3]*row1[1] + filter[6]*row1[2] + filter[1]*row2[0] + filter[4]*row2[1] + filter[7]*row2[2] + filter[2]*row3[0] + filter[5]*row3[1] + filter[8]*row3[2];
				fp[0]= mfac*row2[1] + fac*sqrt(f1*f1 + f2*f2);
				fp++; row1++; row2++; row3++;
			}
		}
	}
}

static void do_filter3(CompBuf *out, CompBuf *in, float *filter, float fac)
{
	float *row1, *row2, *row3;
	float *fp, mfac= 1.0f-fac;
	int rowlen, x, y, c;
	int pixlen= in->type;
	
	rowlen= in->x;
	
	for(y=0; y<in->y; y++) {
		/* setup rows */
		if(y==0) row1= in->rect;
		else row1= in->rect + pixlen*(y-1)*rowlen;
		
		row2= in->rect + y*pixlen*rowlen;
		
		if(y==in->y-1) row3= row2;
		else row3= row2 + pixlen*rowlen;
		
		fp= out->rect + pixlen*(y)*rowlen;
		
		if(pixlen==1) {
			fp[0]= row2[0];
			fp+= 1;
			
			for(x=2; x<rowlen; x++) {
				fp[0]= mfac*row2[1] + fac*(filter[0]*row1[0] + filter[1]*row1[1] + filter[2]*row1[2] + filter[3]*row2[0] + filter[4]*row2[1] + filter[5]*row2[2] + filter[6]*row3[0] + filter[7]*row3[1] + filter[8]*row3[2]);
				fp++; row1++; row2++; row3++;
			}
			fp[0]= row2[1];
		}
		else if(pixlen==2) {
			fp[0]= row2[0];
			fp[1]= row2[1];
			fp+= 2;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<2; c++) {
					fp[0]= mfac*row2[2] + fac*(filter[0]*row1[0] + filter[1]*row1[2] + filter[2]*row1[4] + filter[3]*row2[0] + filter[4]*row2[2] + filter[5]*row2[4] + filter[6]*row3[0] + filter[7]*row3[2] + filter[8]*row3[4]);
					fp++; row1++; row2++; row3++;
				}
			}
			fp[0]= row2[2];
			fp[1]= row2[3];
		}
		else if(pixlen==3) {
			VECCOPY(fp, row2);
			fp+= 3;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<3; c++) {
					fp[0]= mfac*row2[3] + fac*(filter[0]*row1[0] + filter[1]*row1[3] + filter[2]*row1[6] + filter[3]*row2[0] + filter[4]*row2[3] + filter[5]*row2[6] + filter[6]*row3[0] + filter[7]*row3[3] + filter[8]*row3[6]);
					fp++; row1++; row2++; row3++;
				}
			}
			VECCOPY(fp, row2+3);
		}
		else {
			QUATCOPY(fp, row2);
			fp+= 4;
			
			for(x=2; x<rowlen; x++) {
				for(c=0; c<4; c++) {
					fp[0]= mfac*row2[4] + fac*(filter[0]*row1[0] + filter[1]*row1[4] + filter[2]*row1[8] + filter[3]*row2[0] + filter[4]*row2[4] + filter[5]*row2[8] + filter[6]*row3[0] + filter[7]*row3[4] + filter[8]*row3[8]);
					fp++; row1++; row2++; row3++;
				}
			}
			QUATCOPY(fp, row2+4);
		}
	}
}


static void node_composit_exec_filter(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	static float soft[9]= {1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f};
	float sharp[9]= {-1,-1,-1,-1,9,-1,-1,-1,-1};
	float laplace[9]= {-1/8.0f, -1/8.0f, -1/8.0f, -1/8.0f, 1.0f, -1/8.0f, -1/8.0f, -1/8.0f, -1/8.0f};
	float sobel[9]= {1,2,1,0,0,0,-1,-2,-1};
	float prewitt[9]= {1,1,1,0,0,0,-1,-1,-1};
	float kirsch[9]= {5,5,5,-3,-3,-3,-2,-2,-2};
	float shadow[9]= {1,2,1,0,1,0,-1,-2,-1};
	
	if(out[0]->hasoutput==0) return;
	
	/* stack order in: Image */
	/* stack order out: Image */
	
	if(in[1]->data) {
		/* make output size of first available input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 1); // allocs
		
		/* warning note: xof and yof are applied in pixelprocessor, but should be copied otherwise? */
		stackbuf->xof= cbuf->xof;
		stackbuf->yof= cbuf->yof;
		
		switch(node->custom1) {
			case CMP_FILT_SOFT:
				do_filter3(stackbuf, cbuf, soft, in[0]->vec[0]);
				break;
			case CMP_FILT_SHARP:
				do_filter3(stackbuf, cbuf, sharp, in[0]->vec[0]);
				break;
			case CMP_FILT_LAPLACE:
				do_filter3(stackbuf, cbuf, laplace, in[0]->vec[0]);
				break;
			case CMP_FILT_SOBEL:
				do_filter_edge(stackbuf, cbuf, sobel, in[0]->vec[0]);
				break;
			case CMP_FILT_PREWITT:
				do_filter_edge(stackbuf, cbuf, prewitt, in[0]->vec[0]);
				break;
			case CMP_FILT_KIRSCH:
				do_filter_edge(stackbuf, cbuf, kirsch, in[0]->vec[0]);
				break;
			case CMP_FILT_SHADOW:
				do_filter3(stackbuf, cbuf, shadow, in[0]->vec[0]);
				break;
		}
			
		out[0]->data= stackbuf;
	}
}

/* custom1 = filter type */
static bNodeType cmp_node_filter= {
	/* type code   */	CMP_NODE_FILTER,
	/* name        */	"Filter",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_filter_in,
	/* output sock */	cmp_node_filter_out,
	/* storage     */	"", 
	/* execfunc    */	node_composit_exec_filter
	
};


/* **************** VALTORGB ******************** */
static bNodeSocketType cmp_node_valtorgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_valtorgb_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_colorband_composit(bNode *node, float *out, float *in)
{
	do_colorband(node->storage, in[0], out);
}

static void node_composit_exec_valtorgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac */
	/* stack order out: col, alpha */
	
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) 
		return;
	
	if(node->storage) {
		/* input no image? then only color operation */
		if(in[0]->data==NULL) {
			do_colorband(node->storage, in[0]->vec[0], out[0]->vec);
		}
		else {
			/* make output size of input image */
			CompBuf *cbuf= in[0]->data;
			CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
			
			composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_colorband_composit, CB_VAL);
			
			out[0]->data= stackbuf;
			
			if(out[1]->hasoutput)
				out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);

		}
	}
}

static bNodeType cmp_node_valtorgb= {
	/* type code   */	CMP_NODE_VALTORGB,
	/* name        */	"ColorRamp",
	/* width+range */	240, 200, 300,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_valtorgb_in,
	/* output sock */	cmp_node_valtorgb_out,
	/* storage     */	"ColorBand",
	/* execfunc    */	node_composit_exec_valtorgb
	
};


/* **************** RGBTOBW ******************** */
static bNodeSocketType cmp_node_rgbtobw_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_rgbtobw_out[]= {
	{	SOCK_VALUE, 0, "Val",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_rgbtobw(bNode *node, float *out, float *in)
{
	out[0]= in[0]*0.35f + in[1]*0.45f + in[2]*0.2f;
}

static void node_composit_exec_rgbtobw(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */
	
	if(out[0]->hasoutput==0)
		return;
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		do_rgbtobw(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_rgbtobw, CB_RGBA);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_rgbtobw= {
	/* type code   */	CMP_NODE_RGBTOBW,
	/* name        */	"RGB to BW",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	cmp_node_rgbtobw_in,
	/* output sock */	cmp_node_rgbtobw_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rgbtobw
	
};

/* **************** SEPARATE RGBA ******************** */
static bNodeSocketType cmp_node_seprgba_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_seprgba_out[]= {
	{	SOCK_VALUE, 0, "R",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "G",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "B",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "A",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_seprgba(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw channels */
	/* stack order in: col */
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		out[0]->vec[0] = in[0]->vec[0];
		out[1]->vec[0] = in[0]->vec[1];
		out[2]->vec[0] = in[0]->vec[2];
		out[3]->vec[0] = in[0]->vec[3];
	}
	else {
		/* make sure we get right rgba buffer */
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);

		/* don't do any pixel processing, just copy the stack directly (faster, I presume) */
		if(out[0]->hasoutput)
			out[0]->data= valbuf_from_rgbabuf(cbuf, CHAN_R);
		if(out[1]->hasoutput)
			out[1]->data= valbuf_from_rgbabuf(cbuf, CHAN_G);
		if(out[2]->hasoutput)
			out[2]->data= valbuf_from_rgbabuf(cbuf, CHAN_B);
		if(out[3]->hasoutput)
			out[3]->data= valbuf_from_rgbabuf(cbuf, CHAN_A);
		
		if(cbuf!=in[0]->data) 
			free_compbuf(cbuf);

	}
}

static bNodeType cmp_node_seprgba= {
	/* type code   */	CMP_NODE_SEPRGBA,
	/* name        */	"Separate RGBA",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	cmp_node_seprgba_in,
	/* output sock */	cmp_node_seprgba_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_seprgba
	
};

/* **************** SEPARATE HSVA ******************** */
static bNodeSocketType cmp_node_sephsva_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_sephsva_out[]= {
	{	SOCK_VALUE, 0, "H",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "S",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "V",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "A",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_sephsva(bNode *node, float *out, float *in)
{
	float h, s, v;
	
	rgb_to_hsv(in[0], in[1], in[2], &h, &s, &v);
	
	out[0]= h;
	out[1]= s;
	out[2]= v;
	out[3]= in[3];
}

static void node_composit_exec_sephsva(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw channels */
	/* stack order in: col */

	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		float h, s, v;
	
		rgb_to_hsv(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &h, &s, &v);
		
		out[0]->vec[0] = h;
		out[1]->vec[0] = s;
		out[2]->vec[0] = v;
		out[3]->vec[0] = in[0]->vec[3];
	}
	else if ((out[0]->hasoutput) || (out[1]->hasoutput) || (out[2]->hasoutput) || (out[3]->hasoutput)) {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;

		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs

		/* convert the RGB stackbuf to an HSV representation */
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_sephsva, CB_RGBA);

		/* separate each of those channels */
		if(out[0]->hasoutput)
			out[0]->data= valbuf_from_rgbabuf(stackbuf, CHAN_R);
		if(out[1]->hasoutput)
			out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_G);
		if(out[2]->hasoutput)
			out[2]->data= valbuf_from_rgbabuf(stackbuf, CHAN_B);
		if(out[3]->hasoutput)
			out[3]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);
			
		free_compbuf(stackbuf);
	}
}

static bNodeType cmp_node_sephsva= {
	/* type code   */	CMP_NODE_SEPHSVA,
	/* name        */	"Separate HSVA",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	cmp_node_sephsva_in,
	/* output sock */	cmp_node_sephsva_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_sephsva
	
};

/* **************** COMBINE RGBA ******************** */
static bNodeSocketType cmp_node_combrgba_in[]= {
	{	SOCK_VALUE, 1, "R",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "G",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "B",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "A",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_combrgba_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_combrgba(bNode *node, float *out, float *in1, float *in2, float *in3, float *in4)
{
	out[0] = in1[0];
	out[1] = in2[0];
	out[2] = in3[0];
	out[3] = in4[0];
}

static void node_composit_exec_combrgba(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: 1 rgba channels */
	/* stack order in: 4 value channels */
	
	/* input no image? then only color operation */
	if((in[0]->data==NULL) && (in[1]->data==NULL) && (in[2]->data==NULL) && (in[3]->data==NULL)) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[1]->vec[0];
		out[0]->vec[2] = in[2]->vec[0];
		out[0]->vec[3] = in[3]->vec[0];
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf;
		CompBuf *stackbuf;

		/* allocate a CompBuf the size of the first available input */
		if (in[0]->data) cbuf = in[0]->data;
		else if (in[1]->data) cbuf = in[1]->data;
		else if (in[2]->data) cbuf = in[2]->data;
		else cbuf = in[3]->data;
		
		stackbuf = alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
								  in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
								  do_combrgba, CB_VAL, CB_VAL, CB_VAL, CB_VAL);
		
		out[0]->data= stackbuf;
	}	
}

static bNodeType cmp_node_combrgba= {
	/* type code   */	CMP_NODE_COMBRGBA,
	/* name        */	"Combine RGBA",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_combrgba_in,
	/* output sock */	cmp_node_combrgba_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_combrgba
	
};

/* **************** SET ALPHA ******************** */
static bNodeSocketType cmp_node_setalpha_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_setalpha_out[]= {
	{	SOCK_RGBA, 0, "Image",	0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_setalpha(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: RGBA image */
	/* stack order in: col, alpha */
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[0]->vec[1];
		out[0]->vec[2] = in[0]->vec[2];
		out[0]->vec[3] = in[1]->vec[0];
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		if(in[1]->data==NULL && in[1]->vec[0]==1.0f) {
			/* pass on image */
			composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_copy_rgb, CB_RGBA);
		}
		else {
			/* send an compbuf or a value to set as alpha - composit2_pixel_processor handles choosing the right one */
			composit2_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba, CB_RGBA, CB_VAL);
		}
	
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_setalpha= {
	/* type code   */	CMP_NODE_SETALPHA,
	/* name        */	"Set Alpha",
	/* width+range */	120, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_setalpha_in,
	/* output sock */	cmp_node_setalpha_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_setalpha
	
};

/* **************** ALPHAOVER ******************** */
static bNodeSocketType cmp_node_alphaover_in[]= {
	{	SOCK_VALUE, 0, "Fac",			1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_alphaover_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_alphaover_premul(bNode *node, float *out, float *src, float *over, float *fac)
{
	
	if(over[3]<=0.0f) {
		QUATCOPY(out, src);
	}
	else if(*fac==1.0f && over[3]>=1.0f) {
		QUATCOPY(out, over);
	}
	else {
		float mul= 1.0f - *fac*over[3];

		out[0]= (mul*src[0]) + *fac*over[0];
		out[1]= (mul*src[1]) + *fac*over[1];
		out[2]= (mul*src[2]) + *fac*over[2];
		out[3]= (mul*src[3]) + *fac*over[3];
	}	
}

/* result will be still premul, but the over part is premulled */
static void do_alphaover_key(bNode *node, float *out, float *src, float *over, float *fac)
{
	
	if(over[3]<=0.0f) {
		QUATCOPY(out, src);
	}
	else if(*fac==1.0f && over[3]>=1.0f) {
		QUATCOPY(out, over);
	}
	else {
		float premul= fac[0]*over[3];
		float mul= 1.0f - premul;

		out[0]= (mul*src[0]) + premul*over[0];
		out[1]= (mul*src[1]) + premul*over[1];
		out[2]= (mul*src[2]) + premul*over[2];
		out[3]= (mul*src[3]) + fac[0]*over[3];
	}
}


static void node_composit_exec_alphaover(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: col col */
	/* stack order out: col */
	if(out[0]->hasoutput==0) 
		return;
	
	/* input no image? then only color operation */
	if(in[1]->data==NULL) {
		do_alphaover_premul(node, out[0]->vec, in[1]->vec, in[2]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		if(node->custom1)
			composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, in[0]->vec, do_alphaover_key, CB_RGBA, CB_RGBA, CB_VAL);
		else
			composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, in[0]->vec, do_alphaover_premul, CB_RGBA, CB_RGBA, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}

/* custom1: convert 'over' to premul */
static bNodeType cmp_node_alphaover= {
	/* type code   */	CMP_NODE_ALPHAOVER,
	/* name        */	"AlphaOver",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_alphaover_in,
	/* output sock */	cmp_node_alphaover_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_alphaover
	
};

/* **************** Z COMBINE ******************** */
    /* lazy coder note: node->custom1 is abused to send signal */
static bNodeSocketType cmp_node_zcombine_in[]= {
	{	SOCK_RGBA, 1, "Image",		0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 10000.0f},
	{	SOCK_RGBA, 1, "Image",		0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_zcombine_out[]= {
	{	SOCK_RGBA, 0, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 10000.0f},
	{	-1, 0, ""	}
};

static void do_zcombine_mask(bNode *node, float *out, float *z1, float *z2)
{
	if(*z1 > *z2) {
		*out= 1.0f;
		if(node->custom1)
			*z1= *z2;
	}
}

static void do_zcombine_add(bNode *node, float *out, float *col1, float *col2, float *acol)
{
	float alpha= *acol;
	float malpha= 1.0f - alpha;
	
	out[0]= malpha*col1[0] + alpha*col2[0];
	out[1]= malpha*col1[1] + alpha*col2[1];
	out[2]= malpha*col1[2] + alpha*col2[2];
	out[3]= malpha*col1[3] + alpha*col2[3];
}

static void node_composit_exec_zcombine(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: col z col z */
	/* stack order out: col z */
	if(out[0]->hasoutput==0) 
		return;
	
	/* no input image; do nothing now */
	if(in[0]->data==NULL) {
		return;
	}
	else {
		/* make output size of first input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		CompBuf *zbuf, *mbuf;
		float *fp;
		int x;
		char *aabuf;
		
		if(out[1]->hasoutput) {
			/* copy or make a buffer for for the first z value, here we write result in */
			if(in[1]->data)
				zbuf= dupalloc_compbuf(in[1]->data);
			else {
				float *zval;
				int tot= cbuf->x*cbuf->y;
				
				zbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
				for(zval= zbuf->rect; tot; tot--, zval++)
					*zval= in[1]->vec[0];
			}
			/* lazy coder hack */
			node->custom1= 1;
		}
		else {
			node->custom1= 0;
			zbuf= in[1]->data;
		}
		
		/* make a mask based on comparison, optionally write zvalue */
		mbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
		composit2_pixel_processor(node, mbuf, zbuf, in[1]->vec, in[3]->data, in[3]->vec, do_zcombine_mask, CB_VAL, CB_VAL);
		
		/* convert to char */
		aabuf= MEM_mallocN(cbuf->x*cbuf->y, "aa buf");
		fp= mbuf->rect;
		for(x= cbuf->x*cbuf->y-1; x>=0; x--)
			if(fp[x]==0.0f) aabuf[x]= 0;
			else aabuf[x]= 255;
		
		antialias_tagbuf(cbuf->x, cbuf->y, aabuf);
		
		/* convert to float */
		fp= mbuf->rect;
		for(x= cbuf->x*cbuf->y-1; x>=0; x--)
			if(aabuf[x]>1)
				fp[x]= (1.0f/255.0f)*(float)aabuf[x];
		
		composit3_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[2]->data, in[2]->vec, mbuf, NULL, 
								  do_zcombine_add, CB_RGBA, CB_RGBA, CB_VAL);
		/* free */
		free_compbuf(mbuf);
		MEM_freeN(aabuf);
		
		out[0]->data= stackbuf;
		if(node->custom1)
			out[1]->data= zbuf;
	}
}

static bNodeType cmp_node_zcombine= {
	/* type code   */	CMP_NODE_ZCOMBINE,
	/* name        */	"Z Combine",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_zcombine_in,
	/* output sock */	cmp_node_zcombine_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_zcombine
	
};

/* **************** MAP VALUE ******************** */
static bNodeSocketType cmp_node_map_value_in[]= {
	{	SOCK_VALUE, 1, "Value",			1.0f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_map_value_out[]= {
	{	SOCK_VALUE, 0, "Value",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_map_value(bNode *node, float *out, float *src)
{
	TexMapping *texmap= node->storage;
	
	out[0]= (src[0] + texmap->loc[0])*texmap->size[0];
	if(texmap->flag & TEXMAP_CLIP_MIN)
		if(out[0]<texmap->min[0])
			out[0]= texmap->min[0];
	if(texmap->flag & TEXMAP_CLIP_MAX)
		if(out[0]>texmap->max[0])
			out[0]= texmap->max[0];
}

static void node_composit_exec_map_value(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: valbuf */
	/* stack order out: valbuf */
	if(out[0]->hasoutput==0) return;
	
	/* input no image? then only value operation */
	if(in[0]->data==NULL) {
		do_map_value(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_map_value, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_map_value= {
	/* type code   */	CMP_NODE_MAP_VALUE,
	/* name        */	"Map Value",
	/* width+range */	100, 60, 150,
	/* class+opts  */	NODE_CLASS_OP_VECTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_map_value_in,
	/* output sock */	cmp_node_map_value_out,
	/* storage     */	"TexMapping",
	/* execfunc    */	node_composit_exec_map_value
	
};

/* **************** BLUR ******************** */
static bNodeSocketType cmp_node_blur_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Size",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_blur_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static float *make_gausstab(int filtertype, int rad)
{
	float *gausstab, sum, val;
	int i, n;
	
	n = 2 * rad + 1;
	
	gausstab = (float *) MEM_mallocN(n * sizeof(float), "gauss");
	
	sum = 0.0f;
	for (i = -rad; i <= rad; i++) {
		val= RE_filter_value(filtertype, (float)i/(float)rad);
		sum += val;
		gausstab[i+rad] = val;
	}
	
	sum= 1.0f/sum;
	for(i=0; i<n; i++)
		gausstab[i]*= sum;
	
	return gausstab;
}

static float *make_bloomtab(int rad)
{
	float *bloomtab, val;
	int i, n;
	
	n = 2 * rad + 1;
	
	bloomtab = (float *) MEM_mallocN(n * sizeof(float), "bloom");
	
	for (i = -rad; i <= rad; i++) {
		val = pow(1.0 - fabs((float)i)/((float)rad), 4.0);
		bloomtab[i+rad] = val;
	}
	
	return bloomtab;
}

/* both input images of same type, either 4 or 1 channel */
static void blur_single_image(CompBuf *new, CompBuf *img, float scale, NodeBlurData *nbd)
{
	CompBuf *work;
	register float sum, val;
	float rval, gval, bval, aval;
	float *gausstab, *gausstabcent;
	int rad, imgx= img->x, imgy= img->y;
	int x, y, pix= img->type;
	int i, bigstep;
	float *src, *dest;

	/* helper image */
	work= alloc_compbuf(imgx, imgy, img->type, 1); // allocs
	
	/* horizontal */
	rad = scale*(float)nbd->sizex;
	if(rad>imgx/2)
		rad= imgx/2;
	else if(rad<1) 
		rad= 1;

	gausstab= make_gausstab(nbd->filtertype, rad);
	gausstabcent= gausstab+rad;
	
	for (y = 0; y < imgy; y++) {
		float *srcd= img->rect + pix*(y*img->x);
		
		dest = work->rect + pix*(y * img->x);
		
		for (x = 0; x < imgx ; x++) {
			int minr= x-rad<0?-x:-rad;
			int maxr= x+rad>imgx?imgx-x:rad;
			
			src= srcd + pix*(x+minr);
			
			sum= gval = rval= bval= aval= 0.0f;
			for (i= minr; i < maxr; i++) {
				val= gausstabcent[i];
				sum+= val;
				rval += val * (*src++);
				if(pix==4) {
					gval += val * (*src++);
					bval += val * (*src++);
					aval += val * (*src++);
				}
			}
			sum= 1.0f/sum;
			*dest++ = rval*sum;
			if(pix==4) {
				*dest++ = gval*sum;
				*dest++ = bval*sum;
				*dest++ = aval*sum;
			}
		}
	}
	
	/* vertical */
	MEM_freeN(gausstab);
	
	rad = scale*(float)nbd->sizey;
	if(rad>imgy/2)
		rad= imgy/2;
	else if(rad<1) 
		rad= 1;

	gausstab= make_gausstab(nbd->filtertype, rad);
	gausstabcent= gausstab+rad;
	
	bigstep = pix*imgx;
	for (x = 0; x < imgx; x++) {
		float *srcd= work->rect + pix*x;
		
		dest = new->rect + pix*x;
		
		for (y = 0; y < imgy ; y++) {
			int minr= y-rad<0?-y:-rad;
			int maxr= y+rad>imgy?imgy-y:rad;
			
			src= srcd + bigstep*(y+minr);
			
			sum= gval = rval= bval= aval= 0.0f;
			for (i= minr; i < maxr; i++) {
				val= gausstabcent[i];
				sum+= val;
				rval += val * src[0];
				if(pix==4) {
					gval += val * src[1];
					bval += val * src[2];
					aval += val * src[3];
				}
				src += bigstep;
			}
			sum= 1.0f/sum;
			dest[0] = rval*sum;
			if(pix==4) {
				dest[1] = gval*sum;
				dest[2] = bval*sum;
				dest[3] = aval*sum;
			}
			dest+= bigstep;
		}
	}
	
	free_compbuf(work);
	MEM_freeN(gausstab);
}

/* reference has to be mapped 0-1, and equal in size */
static void bloom_with_reference(CompBuf *new, CompBuf *img, CompBuf *ref, float fac, NodeBlurData *nbd)
{
	CompBuf *wbuf;
	register float val;
	float radxf, radyf;
	float **maintabs;
	float *gausstabx, *gausstabcenty;
	float *gausstaby, *gausstabcentx;
	int radx, rady, imgx= img->x, imgy= img->y;
	int x, y;
	int i, j;
	float *src, *dest, *wb;
	
	wbuf= alloc_compbuf(imgx, imgy, CB_VAL, 1);
	
	/* horizontal */
	radx = (float)nbd->sizex;
	if(radx>imgx/2)
		radx= imgx/2;
	else if(radx<1) 
		radx= 1;
	
	/* vertical */
	rady = (float)nbd->sizey;
	if(rady>imgy/2)
		rady= imgy/2;
	else if(rady<1) 
		rady= 1;
	
	x= MAX2(radx, rady);
	maintabs= MEM_mallocN(x*sizeof(void *), "gauss array");
	for(i= 0; i<x; i++)
		maintabs[i]= make_bloomtab(i+1);
		
	/* vars to store before we go */
//	refd= ref->rect;
	src= img->rect;
	
	radxf= (float)radx;
	radyf= (float)rady;
	
	for (y = 0; y < imgy; y++) {
		for (x = 0; x < imgx ; x++, src+=4) {//, refd++) {
			
//			int refradx= (int)(refd[0]*radxf);
//			int refrady= (int)(refd[0]*radyf);
			
			int refradx= (int)(radxf*0.3f*src[3]*(src[0]+src[1]+src[2]));
			int refrady= (int)(radyf*0.3f*src[3]*(src[0]+src[1]+src[2]));
			
			if(refradx>radx) refradx= radx;
			else if(refradx<1) refradx= 1;
			if(refrady>rady) refrady= rady;
			else if(refrady<1) refrady= 1;
			
			if(refradx==1 && refrady==1) {
				wb= wbuf->rect + ( y*imgx + x);
				dest= new->rect + 4*( y*imgx + x);
				wb[0]+= 1.0f;
				dest[0] += src[0];
				dest[1] += src[1];
				dest[2] += src[2];
				dest[3] += src[3];
			}
			else {
				int minxr= x-refradx<0?-x:-refradx;
				int maxxr= x+refradx>imgx?imgx-x:refradx;
				int minyr= y-refrady<0?-y:-refrady;
				int maxyr= y+refrady>imgy?imgy-y:refrady;
				
				float *destd= new->rect + 4*( (y + minyr)*imgx + x + minxr);
				float *wbufd= wbuf->rect + ( (y + minyr)*imgx + x + minxr);
				
				gausstabx= maintabs[refradx-1];
				gausstabcentx= gausstabx+refradx;
				gausstaby= maintabs[refrady-1];
				gausstabcenty= gausstaby+refrady;
				
				for (i= minyr; i < maxyr; i++, destd+= 4*imgx, wbufd+= imgx) {
					dest= destd;
					wb= wbufd;
					for (j= minxr; j < maxxr; j++, dest+=4, wb++) {
						
						val= gausstabcenty[i]*gausstabcentx[j];
						wb[0]+= val;
						dest[0] += val * src[0];
						dest[1] += val * src[1];
						dest[2] += val * src[2];
						dest[3] += val * src[3];
					}
				}
			}
		}
	}
	
	x= imgx*imgy;
	dest= new->rect;
	wb= wbuf->rect;
	while(x--) {
		val= 1.0f/wb[0];
		dest[0]*= val;
		dest[1]*= val;
		dest[2]*= val;
		dest[3]*= val;
		wb++;
		dest+= 4;
	}
	
	free_compbuf(wbuf);
	
	x= MAX2(radx, rady);
	for(i= 0; i<x; i++)
		MEM_freeN(maintabs[i]);
	MEM_freeN(maintabs);
	
}

/* only accepts RGBA buffers */
static void gamma_correct_compbuf(CompBuf *img, int inversed)
{
	float *drect;
	int x;
	
	if(img->type!=CB_RGBA) return;
	
	drect= img->rect;
	if(inversed) {
		for(x=img->x*img->y; x>0; x--, drect+=4) {
			if(drect[0]>0.0f) drect[0]= sqrt(drect[0]); else drect[0]= 0.0f;
			if(drect[1]>0.0f) drect[1]= sqrt(drect[1]); else drect[1]= 0.0f;
			if(drect[2]>0.0f) drect[2]= sqrt(drect[2]); else drect[2]= 0.0f;
		}
	}
	else {
		for(x=img->x*img->y; x>0; x--, drect+=4) {
			if(drect[0]>0.0f) drect[0]*= drect[0]; else drect[0]= 0.0f;
			if(drect[1]>0.0f) drect[1]*= drect[1]; else drect[1]= 0.0f;
			if(drect[2]>0.0f) drect[2]*= drect[2]; else drect[2]= 0.0f;
		}
	}
}
#if 0
static float hexagon_filter(float fi, float fj)
{
	fi= fabs(fi);
	fj= fabs(fj);
	
	if(fj>0.33f) {
		fj= (fj-0.33f)/0.66f;
		if(fi+fj>1.0f)
			return 0.0f;
		else
			return 1.0f;
	}
	else return 1.0f;
}
#endif

/* uses full filter, no horizontal/vertical optimize possible */
/* both images same type, either 1 or 4 channels */
static void bokeh_single_image(CompBuf *new, CompBuf *img, float fac, NodeBlurData *nbd)
{
	register float val;
	float radxf, radyf;
	float *gausstab, *dgauss;
	int radx, rady, imgx= img->x, imgy= img->y;
	int x, y, pix= img->type;
	int i, j, n;
	float *src= NULL, *dest, *srcd= NULL;
	
	/* horizontal */
	radxf = fac*(float)nbd->sizex;
	if(radxf>imgx/2.0f)
		radxf= imgx/2.0f;
	else if(radxf<1.0f) 
		radxf= 1.0f;
	
	/* vertical */
	radyf = fac*(float)nbd->sizey;
	if(radyf>imgy/2.0f)
		radyf= imgy/2.0f;
	else if(radyf<1.0f) 
		radyf= 1.0f;
	
	radx= ceil(radxf);
	rady= ceil(radyf);
	
	n = (2*radx+1)*(2*rady+1);
	
	/* create a full filter image */
	gausstab= MEM_mallocN(sizeof(float)*n, "filter tab");
	dgauss= gausstab;
	val= 0.0f;
	for(j=-rady; j<=rady; j++) {
		for(i=-radx; i<=radx; i++, dgauss++) {
			float fj= (float)j/radyf;
			float fi= (float)i/radxf;
			float dist= sqrt(fj*fj + fi*fi);
			
		//*dgauss= hexagon_filter(fi, fj);
			*dgauss= RE_filter_value(nbd->filtertype, 2.0f*dist - 1.0f);

			val+= *dgauss;
		}
	}

	if(val!=0.0f) {
		val= 1.0f/val;
		for(j= n -1; j>=0; j--)
			gausstab[j]*= val;
	}
	else gausstab[4]= 1.0f;
	
	for (y = -rady+1; y < imgy+rady-1; y++) {
		
		if(y<=0) srcd= img->rect;
		else if(y<imgy) srcd+= pix*imgx;
		else srcd= img->rect + pix*(imgy-1)*imgx;
			
		for (x = -radx+1; x < imgx+radx-1 ; x++) {
			int minxr= x-radx<0?-x:-radx;
			int maxxr= x+radx>=imgx?imgx-x-1:radx;
			int minyr= y-rady<0?-y:-rady;
			int maxyr= y+rady>imgy-1?imgy-y-1:rady;
			
			float *destd= new->rect + pix*( (y + minyr)*imgx + x + minxr);
			float *dgausd= gausstab + (minyr+rady)*2*radx + minxr+radx;
			
			if(x<=0) src= srcd;
			else if(x<imgx) src+= pix;
			else src= srcd + pix*(imgx-1);
			
			for (i= minyr; i <=maxyr; i++, destd+= pix*imgx, dgausd+= 2*radx + 1) {
				dest= destd;
				dgauss= dgausd;
				for (j= minxr; j <=maxxr; j++, dest+=pix, dgauss++) {
					val= *dgauss;
					if(val!=0.0f) {
						dest[0] += val * src[0];
						if(pix>1) {
							dest[1] += val * src[1];
							dest[2] += val * src[2];
							dest[3] += val * src[3];
						}
					}
				}
			}
		}
	}
	
	MEM_freeN(gausstab);
}


/* reference has to be mapped 0-1, and equal in size */
static void blur_with_reference(CompBuf *new, CompBuf *img, CompBuf *ref, NodeBlurData *nbd)
{
	CompBuf *blurbuf, *ref_use;
	register float sum, val;
	float rval, gval, bval, aval, radxf, radyf;
	float **maintabs;
	float *gausstabx, *gausstabcenty;
	float *gausstaby, *gausstabcentx;
	int radx, rady, imgx= img->x, imgy= img->y;
	int x, y, pix= img->type;
	int i, j;
	float *src, *dest, *refd, *blurd;

	if(ref->x!=img->x && ref->y!=img->y)
		return;
	
	ref_use= typecheck_compbuf(ref, CB_VAL);
	
	/* trick is; we blur the reference image... but only works with clipped values*/
	blurbuf= alloc_compbuf(imgx, imgy, CB_VAL, 1);
	blurd= blurbuf->rect;
	refd= ref_use->rect;
	for(x= imgx*imgy; x>0; x--, refd++, blurd++) {
		if(refd[0]<0.0f) blurd[0]= 0.0f;
		else if(refd[0]>1.0f) blurd[0]= 1.0f;
		else blurd[0]= refd[0];
	}
	
	blur_single_image(blurbuf, blurbuf, 1.0f, nbd);
	
	/* horizontal */
	radx = (float)nbd->sizex;
	if(radx>imgx/2)
		radx= imgx/2;
	else if(radx<1) 
		radx= 1;
	
	/* vertical */
	rady = (float)nbd->sizey;
	if(rady>imgy/2)
		rady= imgy/2;
	else if(rady<1) 
		rady= 1;
	
	x= MAX2(radx, rady);
	maintabs= MEM_mallocN(x*sizeof(void *), "gauss array");
	for(i= 0; i<x; i++)
		maintabs[i]= make_gausstab(nbd->filtertype, i+1);
	
	refd= blurbuf->rect;
	dest= new->rect;
	radxf= (float)radx;
	radyf= (float)rady;
	
	for (y = 0; y < imgy; y++) {
		for (x = 0; x < imgx ; x++, dest+=pix, refd++) {
			int refradx= (int)(refd[0]*radxf);
			int refrady= (int)(refd[0]*radyf);
			
			if(refradx>radx) refradx= radx;
			else if(refradx<1) refradx= 1;
			if(refrady>rady) refrady= rady;
			else if(refrady<1) refrady= 1;

			if(refradx==1 && refrady==1) {
				src= img->rect + pix*( y*imgx + x);
				if(pix==1)
					dest[0]= src[0];
				else
					QUATCOPY(dest, src);
			}
			else {
				int minxr= x-refradx<0?-x:-refradx;
				int maxxr= x+refradx>imgx?imgx-x:refradx;
				int minyr= y-refrady<0?-y:-refrady;
				int maxyr= y+refrady>imgy?imgy-y:refrady;
	
				float *srcd= img->rect + pix*( (y + minyr)*imgx + x + minxr);
				
				gausstabx= maintabs[refradx-1];
				gausstabcentx= gausstabx+refradx;
				gausstaby= maintabs[refrady-1];
				gausstabcenty= gausstaby+refrady;

				sum= gval = rval= bval= aval= 0.0f;
				
				for (i= minyr; i < maxyr; i++, srcd+= pix*imgx) {
					src= srcd;
					for (j= minxr; j < maxxr; j++, src+=pix) {
					
						val= gausstabcenty[i]*gausstabcentx[j];
						sum+= val;
						rval += val * src[0];
						if(pix>1) {
							gval += val * src[1];
							bval += val * src[2];
							aval += val * src[3];
						}
					}
				}
				sum= 1.0f/sum;
				dest[0] = rval*sum;
				if(pix>1) {
					dest[1] = gval*sum;
					dest[2] = bval*sum;
					dest[3] = aval*sum;
				}
			}
		}
	}
	
	free_compbuf(blurbuf);
	
	x= MAX2(radx, rady);
	for(i= 0; i<x; i++)
		MEM_freeN(maintabs[i]);
	MEM_freeN(maintabs);
	
	if(ref_use!=ref)
		free_compbuf(ref_use);
}



static void node_composit_exec_blur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *img= in[0]->data;
	
	if(img==NULL || out[0]->hasoutput==0)
		return;
	
	if(img->type==CB_VEC2 || img->type==CB_VEC3) {
		img= typecheck_compbuf(in[0]->data, CB_RGBA);
	}
	
	/* if fac input, we do it different */
	if(in[1]->data) {
		
		/* make output size of input image */
		new= alloc_compbuf(img->x, img->y, img->type, 1); // allocs
		
		blur_with_reference(new, img, in[1]->data, node->storage);
		
		out[0]->data= new;
	}
	else {
		
		if(in[1]->vec[0]<=0.001f) {	/* time node inputs can be a tiny value */
			new= pass_on_compbuf(img);
		}
		else {
			NodeBlurData *nbd= node->storage;
			CompBuf *gammabuf;
			
			/* make output size of input image */
			new= alloc_compbuf(img->x, img->y, img->type, 1); // allocs
			
			if(nbd->gamma) {
				gammabuf= dupalloc_compbuf(img);
				gamma_correct_compbuf(gammabuf, 0);
			}
			else gammabuf= img;
			
			if(nbd->bokeh)
				bokeh_single_image(new, gammabuf, in[1]->vec[0], nbd);
			else if(1)
				blur_single_image(new, gammabuf, in[1]->vec[0], nbd);
			else	/* bloom experimental... */
				bloom_with_reference(new, gammabuf, NULL, in[1]->vec[0], nbd);
			
			if(nbd->gamma) {
				gamma_correct_compbuf(new, 1);
				free_compbuf(gammabuf);
			}
		}
		out[0]->data= new;
	}
	if(img!=in[0]->data)
		free_compbuf(img);
}
	

static bNodeType cmp_node_blur= {
	/* type code   */	CMP_NODE_BLUR,
	/* name        */	"Blur",
	/* width+range */	120, 80, 200,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_blur_in,
	/* output sock */	cmp_node_blur_out,
	/* storage     */	"NodeBlurData",
	/* execfunc    */	node_composit_exec_blur
	
};

/* ************ qdn: Defocus node ****************** */
static bNodeSocketType cmp_node_defocus_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_defocus_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


// line coefs for point sampling & scancon. data.
typedef struct BokehCoeffs {
	float x0, y0, dx, dy;
	float ls_x, ls_y;
	float min_x, min_y, max_x, max_y;
} BokehCoeffs;

// returns array of BokehCoeffs
// returns length of array in 'len_bkh',
// radius squared of inscribed disk in 'inradsq', needed in getWeight() test,
// BKH[8] is the data returned for the bokeh shape & bkh_b[4] is it's 2d bound
static void makeBokeh(char bktype, char ro, int* len_bkh, float* inradsq, BokehCoeffs BKH[8], float bkh_b[4])
{
	float x0, x1, y0, y1, dx, dy, iDxy, w = ro*M_PI/180.f;
	float wi = (360.f/bktype)*M_PI/180.f;
	int i, ov, nv;
	
	// bktype must be at least 3 & <= 8
	bktype = (bktype<3) ? 3 : ((bktype>8) ? 8 : bktype);
	*len_bkh = bktype;
	*inradsq = -1.f;

	for (i=0; i<(*len_bkh); i++) {
		x0 = cos(w);
		y0 = sin(w);
		w += wi;
		x1 = cos(w);
		y1 = sin(w);
		if ((*inradsq)<0.f) {
			// radius squared of inscribed disk
			float idx=(x0+x1)*0.5f, idy=(y0+y1)*0.5f;
			*inradsq = idx*idx + idy*idy;
		}
		BKH[i].x0 = x0;
		BKH[i].y0 = y0;
		dx = x1-x0, dy = y1-y0;
		iDxy = 1.f / sqrt(dx*dx + dy*dy);
		dx *= iDxy;
		dy *= iDxy;
		BKH[i].dx = dx;
		BKH[i].dy = dy;
	}

	// precalc scanconversion data
	// bokeh bound, not transformed, for scanconvert
	bkh_b[0] = bkh_b[2] = 1e10f;	// xmin/ymin
	bkh_b[1] = bkh_b[3] = -1e10f;	// xmax/ymax
	ov = (*len_bkh) - 1;
	for (nv=0; nv<(*len_bkh); nv++) {
		bkh_b[0] = MIN2(bkh_b[0], BKH[nv].x0);	// xmin
		bkh_b[1] = MAX2(bkh_b[1], BKH[nv].x0);	// xmax
		bkh_b[2] = MIN2(bkh_b[2], BKH[nv].y0);	// ymin
		bkh_b[3] = MAX2(bkh_b[3], BKH[nv].y0);	// ymax
		BKH[nv].min_x = MIN2(BKH[ov].x0, BKH[nv].x0);
		BKH[nv].max_x = MAX2(BKH[ov].x0, BKH[nv].x0);
		BKH[nv].min_y = MIN2(BKH[ov].y0, BKH[nv].y0);
		BKH[nv].max_y = MAX2(BKH[ov].y0, BKH[nv].y0);
		dy = BKH[nv].y0 - BKH[ov].y0;
		BKH[nv].ls_x = (BKH[nv].x0 - BKH[ov].x0) / ((dy==0.f) ? 1.f : dy);
		BKH[nv].ls_y = (BKH[nv].ls_x==0.f) ? 1.f : (1.f/BKH[nv].ls_x);
		ov = nv;
	}
}

// test if u/v inside shape & returns weight value
static float getWeight(BokehCoeffs* BKH, int len_bkh, float u, float v, float rad, float inradsq)
{
	BokehCoeffs* bc = BKH;
	float cdist, irad = (rad==0.f) ? 1.f : (1.f/rad);
	u *= irad;
	v *= irad;
 
	// early out test1: if point outside outer unit disk, it cannot be inside shape
	cdist = u*u + v*v;
	if (cdist>1.f) return 0.f;
	
	// early out test2: if point inside or on inner disk, point must be inside shape
	if (cdist<=inradsq) return 1.f;
	
	while (len_bkh--) {
		if ((bc->dy*(u - bc->x0) - bc->dx*(v - bc->y0)) > 0.f) return 0.f;
		bc++;
	}
	return 1.f;
}

// QMC.seq. for sampling, A.Keller, EMS
static float RI_vdC(unsigned int bits, unsigned int r)
{
	bits = ( bits << 16) | ( bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);
	bits ^= r;
	return (float)((double)bits / 4294967296.0);
}

// single channel IIR gaussian filtering
// much faster than anything else, constant time independent of width
// should extend to multichannel and make this a node, could be useful
static void IIR_gauss(CompBuf* buf, float sigma)
{
	double q, q2, sc, cf[4], tsM[9], tsu[3], tsv[3];
	float *X, *Y, *W;
	int i, x, y, sz;

	// single channel only for now
	if (buf->type != CB_VAL) return;

	// <0.5 not valid, though can have a possibly useful sort of sharpening effect
	if (sigma < 0.5) return;
	
	// see "Recursive Gabor Filtering" by Young/VanVliet
	// all factors here in double.prec. Required, because for single.prec it seems to blow up if sigma > ~200
	if (sigma >= 3.556)
		q = 0.9804*(sigma - 3.556) + 2.5091;
	else // sigma >= 0.5
		q = (0.0561*sigma + 0.5784)*sigma - 0.2568;
	q2 = q*q;
	sc = (1.1668 + q)*(3.203729649  + (2.21566 + q)*q);
	// no gabor filtering here, so no complex multiplies, just the regular coefs.
	// all negated here, so as not to have to recalc Triggs/Sdika matrix
	cf[1] = q*(5.788961737 + (6.76492 + 3.0*q)*q)/ sc;
	cf[2] = -q2*(3.38246 + 3.0*q)/sc;
	// 0 & 3 unchanged
	cf[3] = q2*q/sc;
	cf[0] = 1.0 - cf[1] - cf[2] - cf[3];

	// Triggs/Sdika border corrections,
	// it seems to work, not entirely sure if it is actually totally correct,
	// Besides J.M.Geusebroek's anigauss.c (see http://www.science.uva.nl/~mark),
	// found one other implementation by Cristoph Lampert,
	// but neither seem to be quite the same, result seems to be ok sofar anyway.
	// Extra scale factor here to not have to do it in filter,
	// though maybe this had something to with the precision errors
	sc = cf[0]/((1.0 + cf[1] - cf[2] + cf[3])*(1.0 - cf[1] - cf[2] - cf[3])*(1.0 + cf[2] + (cf[1] - cf[3])*cf[3]));
	tsM[0] = sc*(-cf[3]*cf[1] + 1.0 - cf[3]*cf[3] - cf[2]);
	tsM[1] = sc*((cf[3] + cf[1])*(cf[2] + cf[3]*cf[1]));
	tsM[2] = sc*(cf[3]*(cf[1] + cf[3]*cf[2]));
	tsM[3] = sc*(cf[1] + cf[3]*cf[2]);
	tsM[4] = sc*(-(cf[2] - 1.0)*(cf[2] + cf[3]*cf[1]));
	tsM[5] = sc*(-(cf[3]*cf[1] + cf[3]*cf[3] + cf[2] - 1.0)*cf[3]);
	tsM[6] = sc*(cf[3]*cf[1] + cf[2] + cf[1]*cf[1] - cf[2]*cf[2]);
	tsM[7] = sc*(cf[1]*cf[2] + cf[3]*cf[2]*cf[2] - cf[1]*cf[3]*cf[3] - cf[3]*cf[3]*cf[3] - cf[3]*cf[2] + cf[3]);
	tsM[8] = sc*(cf[3]*(cf[1] + cf[3]*cf[2]));

#define YVV(L)\
{\
	W[0] = cf[0]*X[0] + cf[1]*X[0] + cf[2]*X[0] + cf[3]*X[0];\
	W[1] = cf[0]*X[1] + cf[1]*W[0] + cf[2]*X[0] + cf[3]*X[0];\
	W[2] = cf[0]*X[2] + cf[1]*W[1] + cf[2]*W[0] + cf[3]*X[0];\
	for (i=3; i<L; i++)\
		W[i] = cf[0]*X[i] + cf[1]*W[i-1] + cf[2]*W[i-2] + cf[3]*W[i-3];\
	tsu[0] = W[L-1] - X[L-1];\
	tsu[1] = W[L-2] - X[L-1];\
	tsu[2] = W[L-3] - X[L-1];\
	tsv[0] = tsM[0]*tsu[0] + tsM[1]*tsu[1] + tsM[2]*tsu[2] + X[L-1];\
	tsv[1] = tsM[3]*tsu[0] + tsM[4]*tsu[1] + tsM[5]*tsu[2] + X[L-1];\
	tsv[2] = tsM[6]*tsu[0] + tsM[7]*tsu[1] + tsM[8]*tsu[2] + X[L-1];\
	Y[L-1] = cf[0]*W[L-1] + cf[1]*tsv[0] + cf[2]*tsv[1] + cf[3]*tsv[2];\
	Y[L-2] = cf[0]*W[L-2] + cf[1]*Y[L-1] + cf[2]*tsv[0] + cf[3]*tsv[1];\
	Y[L-3] = cf[0]*W[L-3] + cf[1]*Y[L-2] + cf[2]*Y[L-1] + cf[3]*tsv[0];\
	for (i=L-4; i>=0; i--)\
		Y[i] = cf[0]*W[i] + cf[1]*Y[i+1] + cf[2]*Y[i+2] + cf[3]*Y[i+3];\
}

	// intermediate buffers
	sz = MAX2(buf->x, buf->y);
	Y = MEM_callocN(sz*sizeof(float), "IIR_gauss Y buf");
	W = MEM_callocN(sz*sizeof(float), "IIR_gauss W buf");
	// H
	for (y=0; y<buf->y; y++) {
		X = &buf->rect[y*buf->x];
		YVV(buf->x);
		memcpy(X, Y, sizeof(float)*buf->x);
	}
	// V
	X = MEM_callocN(buf->y*sizeof(float), "IIR_gauss X buf");
	for (x=0; x<buf->x; x++) {
		for (y=0; y<buf->y; y++)
			X[y] = buf->rect[x + y*buf->x];
		YVV(buf->y);
		for (y=0; y<buf->y; y++)
			buf->rect[x + y*buf->x] = Y[y];
	}
	MEM_freeN(X);

	MEM_freeN(W);
	MEM_freeN(Y);
#undef YVV
}

static void defocus_blur(CompBuf* new, CompBuf* img, CompBuf* zbuf, float inpval, NodeDefocus* nqd)
{
	CompBuf *wts;	// weights buffer
	CompBuf *crad;	// CoC radius buffer
	BokehCoeffs BKH[8];	// bokeh shape data, here never > 8 pts.
	float bkh_b[4] = {0};	// shape 2D bound
	unsigned int p, px, p4, zp, cp, cp4;
	float *ctcol, u, v, iZ, ct_crad, bcrad, lwt, wt=0, cR2=0;
	float dof_sp, maxfgc, nmaxc, scf, bk_hn_theta=0, inradsq=0;
	float cam_fdist=1, cam_invfdist=1, cam_lens=35;
	int x, y, sx, sy, len_bkh=0;
	float aspect, aperture;
	int minsz;
	
	// get some required params from the current scene camera
	Object* camob = G.scene->camera;
	if (camob->type==OB_CAMERA) {
		Camera* cam = (Camera*)camob->data;
		cam_lens = cam->lens;
		cam_fdist = (cam->YF_dofdist==0.f) ? 1e10f : cam->YF_dofdist;
		cam_invfdist = 1.f/cam_fdist;
	}

	// guess work here.. best match with raytraced result
	minsz = MIN2(img->x, img->y);
	dof_sp = (float)minsz / (16.f / cam_lens);	// <- == aspect * MIN2(img->x, img->y) / tan(0.5f * fov);
	
	// aperture
	aspect = (img->x > img->y) ? (img->y / (float)img->x) : (img->x / (float)img->y);
	aperture = 0.5f*(cam_lens / (aspect*32.f)) / nqd->fstop;
	
	// if not disk, make bokeh coefficients and other needed data
	if (nqd->bktype!=0) {
		makeBokeh(nqd->bktype, nqd->rotation, &len_bkh, &inradsq, BKH, bkh_b);
		bk_hn_theta = 0.5 * nqd->bktype * sin(2.0 * M_PI / nqd->bktype);	// weight factor
	}
	
	// accumulated weights
	wts = alloc_compbuf(img->x, img->y, CB_VAL, 1);
	// CoC radius buffer
	crad = alloc_compbuf(img->x, img->y, CB_VAL, 1);

	// if 'no_zbuf' flag set (which is always set if input is not an image),
	// values are instead interpreted directly as blur radius values
	if (nqd->no_zbuf) {
		for (p=0; p<(unsigned int)(img->x*img->y); p++) {
			crad->rect[p] = zbuf ? (zbuf->rect[p]*nqd->scale) : inpval;
			if (crad->rect[p] < 0.01f) crad->rect[p] = 0.01f;
			// if maxblur!=0, limit maximum
			if (nqd->maxblur != 0.f) crad->rect[p] = MIN2(crad->rect[p], nqd->maxblur);
		}
	}
	else {
		// actual zbuffer.
		// separate foreground from background CoC's
		// then blur background and blend in again with foreground,
		// improves the 'blurred foreground overlapping in-focus midground' sharp boundary problem.
		// wts buffer here used for blendmask
		maxfgc = 0.f; // maximum foreground CoC radius
		for (y=0; y<img->y; y++) {
			p = y * img->x;
			for (x=0; x<img->x; x++) {
				px = p + x;
				iZ = (zbuf->rect[px]==0.f) ? 0.f : (1.f/zbuf->rect[px]);
				crad->rect[px] = 0.5f*(aperture*(dof_sp*(cam_invfdist - iZ) - 1.f));
				if (crad->rect[px] <= 0.f) {
					wts->rect[px] = 1.f;
					crad->rect[px] = -crad->rect[px];
					if (crad->rect[px] > maxfgc) maxfgc = crad->rect[px];
				}
				else crad->rect[px] = wts->rect[px] = 0;
			}
		}
		
		// fast blur...
		IIR_gauss(crad, 2.f*maxfgc);
		IIR_gauss(wts, 2.f*maxfgc);
		
		// find new maximum to scale it back to original
		// (could skip this, not strictly necessary, in general, difference is quite small, but just in case...)
		nmaxc = 0;
		for (p=0; p<(img->x*img->y); p++)
			if (crad->rect[p] > nmaxc) nmaxc = crad->rect[p];
		// rescale factor
		scf = (nmaxc==0.f) ? 1.f: (maxfgc / nmaxc);
		
		// and blend...
		for (y=0; y<img->y; y++) {
			p = y*img->x;
			for (x=0; x<img->x; x++) {
				px = p + x;
				iZ = (zbuf->rect[px]==0.f) ? 0.f : (1.f/zbuf->rect[px]);
				bcrad = 0.5f*fabs(aperture*(dof_sp*(cam_invfdist - iZ) - 1.f));
				// scale crad back to original maximum and blend
				crad->rect[px] = bcrad + wts->rect[px]*(scf*crad->rect[px] - bcrad);
				if (crad->rect[px] < 0.01f) crad->rect[px] = 0.01f;
				// if maxblur!=0, limit maximum
				if (nqd->maxblur != 0.f) crad->rect[px] = MIN2(crad->rect[px], nqd->maxblur);
				// clear weights for next part
				wts->rect[px] = 0.f;
			}
		}
		
	}

	//------------------------------------------------------------------
	// main loop
	for (y=0; y<img->y; y++) {
		// some sort of visual feedback would be nice, or at least this text in the renderwin header
		// but for now just print some info in the console every 8 scanlines.
		if (((y & 7)==0) || (y==(img->y-1))) {
			printf("\rdefocus: Processing Line %d of %d ... ", y+1, img->y);
			fflush(stdout);
		}
		zp = y * img->x;
		for (x=0; x<img->x; x++) {
			cp = zp + x;
			cp4 = cp * img->type;

			// Circle of Confusion radius for current pixel
			cR2 = ct_crad = crad->rect[cp];
			cR2 *= cR2;
			
			// pixel color
			ctcol = &img->rect[cp4];
			
			if (!nqd->preview) {
				int xs, xe, ys, ye;
				float lwt, wtcol[4] = {0}, aacol[4] = {0};

				// shape weight
				if (nqd->bktype==0)	// disk
					wt = 1.f/((float)M_PI*cR2);
				else
					wt = 1.f/(cR2*bk_hn_theta);

				// weighted color
				wtcol[0] = wt*ctcol[0];
				if (new->type != CB_VAL) {
					wtcol[1] = wt*ctcol[1];
					wtcol[2] = wt*ctcol[2];
					wtcol[3] = wt*ctcol[3];
				}

				// macro for background blur overlap test
				// unfortunately, since this is done per pixel,
				// it has a very significant negative impact on processing time...
				// (eg. aa disk blur without test: 112 sec, vs with test: 176 sec...)
				// iff center blur radius > threshold
				// and if overlap pixel in focus, do nothing, else add color/weigbt
				// (threshold constant is dependant on amount of blur)
				#define TESTBG1(c, w) {\
					if (ct_crad > nqd->bthresh) {\
						if (crad->rect[p] > nqd->bthresh) {\
							new->rect[p] += c[0];\
							wts->rect[p] += w;\
						}\
					}\
					else {\
						new->rect[p] += c[0];\
						wts->rect[p] += w;\
					}\
				}
				#define TESTBG4(c, w) {\
					if (ct_crad > nqd->bthresh) {\
						if (crad->rect[p] > nqd->bthresh) {\
							new->rect[p4] += c[0];\
							new->rect[p4+1] += c[1];\
							new->rect[p4+2] += c[2];\
							new->rect[p4+3] += c[3];\
							wts->rect[p] += w;\
						}\
					}\
					else {\
						new->rect[p4] += c[0];\
						new->rect[p4+1] += c[1];\
						new->rect[p4+2] += c[2];\
						new->rect[p4+3] += c[3];\
						wts->rect[p] += w;\
					}\
				}
				if (nqd->bktype == 0) {
					// Disk
					int _x, i, j, di;
					float Dj, T;
					// AA pixel
					#define AAPIX(a, b) {\
						int _ny = b;\
						if ((_ny >= 0) && (_ny < new->y)) {\
							int _nx = a;\
							if ((_nx >=0) && (_nx < new->x)) {\
								p = _ny*new->x + _nx;\
								if (new->type==CB_VAL) {\
									TESTBG1(aacol, lwt);\
								}\
								else {\
									p4 = p * new->type;\
									TESTBG4(aacol, lwt);\
								}\
							}\
						}\
					}
					// circle scanline
					#define CSCAN(a, b) {\
						int _ny = y + b;\
						if ((_ny >= 0) && (_ny < new->y)) {\
							xs = x - a + 1;\
							if (xs < 0) xs = 0;\
							xe = x + a;\
							if (xe > new->x) xe = new->x;\
							p = _ny*new->x + xs;\
							if (new->type==CB_VAL) {\
								for (_x=xs; _x<xe; _x++, p++) TESTBG1(wtcol, wt);\
							}\
							else {\
								p4 = p * new->type;\
								for (_x=xs; _x<xe; _x++, p++, p4+=new->type) TESTBG4(wtcol, wt);\
							}\
						}\
					}
					i = ceil(ct_crad);
					j = 0;
					T = 0;
					while (i > j) {
						Dj = sqrt(cR2 - j*j);
						Dj -= floor(Dj);
						di = 0;
						if (Dj > T) { i--;  di = 1; }
						T = Dj;
						aacol[0] = wtcol[0]*Dj;
						if (new->type != CB_VAL) {
							aacol[1] = wtcol[1]*Dj;
							aacol[2] = wtcol[2]*Dj;
							aacol[3] = wtcol[3]*Dj;
						}
						lwt = wt*Dj;
						if (i!=j) {
							// outer pixels
							AAPIX(x+j, y+i);
							AAPIX(x+j, y-i);
							if (j) {
								AAPIX(x-j, y+i); // BL
								AAPIX(x-j, y-i); // TL
							}
							if (di) { // only when i changed, interior of outer section
								CSCAN(j, i); // bottom
								CSCAN(j, -i); // top
							}
						}
						// lower mid section
						AAPIX(x+i, y+j);
						if (i) AAPIX(x-i, y+j);
						CSCAN(i, j);
						// upper mid section
						if (j) {
							AAPIX(x+i, y-j);
							if (i) AAPIX(x-i, y-j);
							CSCAN(i, -j);
						}
						j++;
					}
					#undef CSCAN
					#undef AAPIX
				}
				else {
					// n-agonal
					int ov, nv;
					float mind, maxd, lwt;
					ys = MAX2((int)floor(bkh_b[2]*ct_crad + y), 0);
					ye = MIN2((int)ceil(bkh_b[3]*ct_crad + y), new->y - 1);
					for (sy=ys; sy<=ye; sy++) {
						float fxs = 1e10f, fxe = -1e10f;
						float yf = (sy - y)/ct_crad;
						int found = 0;
						ov = len_bkh - 1;
						mind = maxd = 0;
						for (nv=0; nv<len_bkh; nv++) {
							if ((BKH[nv].max_y >= yf) && (BKH[nv].min_y <= yf)) {
								float tx = BKH[ov].x0 + BKH[nv].ls_x*(yf - BKH[ov].y0);
								if (tx < fxs) { fxs = tx;  mind = BKH[nv].ls_x; }
								if (tx > fxe) { fxe = tx;  maxd = BKH[nv].ls_x; }
								if (++found == 2) break;
							}
							ov = nv;
						}
						if (found) {
							fxs = fxs*ct_crad + x;
							fxe = fxe*ct_crad + x;
							xs = (int)floor(fxs), xe = (int)ceil(fxe);
							// AA hack for first and last x pixel, near vertical edges only
							if (fabs(mind) <= 1.f) {
								if ((xs >= 0) && (xs < new->x)) {
									lwt = 1.f-(fxs - xs);
									aacol[0] = wtcol[0]*lwt;
									p = xs + sy*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
							if (fabs(maxd) <= 1.f) {
								if ((xe >= 0) && (xe < new->x)) {
									lwt = 1.f-(xe - fxe);
									aacol[0] = wtcol[0]*lwt;
									p = xe + sy*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
							xs = MAX2(xs+1, 0);
							xe = MIN2(xe, new->x);
							// remaining interior scanline
							p = sy*new->x + xs;
							if (new->type==CB_VAL) {
								for (sx=xs; sx<xe; sx++, p++) TESTBG1(wtcol, wt);
							}
							else {
								p4 = p * new->type;
								for (sx=xs; sx<xe; sx++, p++, p4+=new->type) TESTBG4(wtcol, wt);
							}
						}
					}

					// now traverse in opposite direction, y scanlines,
					// but this time only draw the near horizontal edges,
					// applying same AA hack as above
					xs = MAX2((int)floor(bkh_b[0]*ct_crad + x), 0);
					xe = MIN2((int)ceil(bkh_b[1]*ct_crad + x), img->x - 1);
					for (sx=xs; sx<=xe; sx++) {
						float xf = (sx - x)/ct_crad;
						float fys = 1e10f, fye = -1e10f;
						int found = 0;
						ov = len_bkh - 1;
						mind = maxd = 0;
						for (nv=0; nv<len_bkh; nv++) {
							if ((BKH[nv].max_x >= xf) && (BKH[nv].min_x <= xf)) {
								float ty = BKH[ov].y0 + BKH[nv].ls_y*(xf - BKH[ov].x0);
								if (ty < fys) { fys = ty;  mind = BKH[nv].ls_y; }
								if (ty > fye) { fye = ty;  maxd = BKH[nv].ls_y; }
								if (++found == 2) break;
							}
							ov = nv;
						}
						if (found) {
							fys = fys*ct_crad + y;
							fye = fye*ct_crad + y;
							// near horizontal edges only, line slope <= 1
							if (fabs(mind) <= 1.f) {
								int iys = (int)floor(fys);
								if ((iys >= 0) && (iys < new->y)) {
									lwt = 1.f - (fys - iys);
									aacol[0] = wtcol[0]*lwt;
									p = sx + iys*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
							if (fabs(maxd) <= 1.f) {
								int iye = ceil(fye);
								if ((iye >= 0) && (iye < new->y)) {
									lwt = 1.f - (iye - fye);
									aacol[0] = wtcol[0]*lwt;
									p = sx + iye*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
						}
					}

				}
				#undef TESTBG4
				#undef TESTBG1

			}
			else {
				// sampled, simple rejection sampling here, good enough
				unsigned int maxsam, s, ui = BLI_rand()*BLI_rand();
				float cpr = BLI_frand();
				float wcor;
				
				if (nqd->no_zbuf)
					maxsam = nqd->samples;	// no zbuffer input, use sample value directly
				else {
				  // depth adaptive sampling hack, the more out of focus, the more samples taken, 16 minimum.
					maxsam = (int)(0.5f + nqd->samples*(1.f-(float)exp(-fabs(zbuf->rect[cp] - cam_fdist))));
					if (maxsam < 16) maxsam = 16;
				}
				
				wcor = 1.f/(float)maxsam;
				
				for (s=0; s<maxsam; ++s) {
					u = ct_crad*(2.f*RI_vdC(s, ui) - 1.f);
					v = ct_crad*(2.f*(s + cpr)/(float)maxsam - 1.f);
					// should use extra 0.5 offset here, but will cause gap around focal point...
					sx = (int)(x + u), sy = (int)(y + v);
					if ((sx<0) || (sx >= new->x) || (sy<0) || (sy >= new->y)) continue;
					p = sx + sy*new->x;
					p4 = p * new->type;
					if (nqd->bktype==0)	// Disk
						lwt = ((u*u + v*v)<=cR2) ? wcor : 0.f;
					else	// AA not needed here
						lwt = wcor * getWeight(BKH, len_bkh, u, v, ct_crad, inradsq);
					// prevent background bleeding onto in-focus pixels, user-option
					if (ct_crad > nqd->bthresh) {  // if center blur > threshold
						if (crad->rect[p] > nqd->bthresh) { // if overlap pixel in focus, do nothing, else add color/weigbt
							new->rect[p4] += ctcol[0] * lwt;
							if (new->type != CB_VAL) {
								new->rect[p4+1] += ctcol[1] * lwt;
								new->rect[p4+2] += ctcol[2] * lwt;
								new->rect[p4+3] += ctcol[3] * lwt;
							}
							wts->rect[p] += lwt;
						}
					}
					else {
						new->rect[p4] += ctcol[0] * lwt;
						if (new->type != CB_VAL) {
							new->rect[p4+1] += ctcol[1] * lwt;
							new->rect[p4+2] += ctcol[2] * lwt;
							new->rect[p4+3] += ctcol[3] * lwt;
						}
						wts->rect[p] += lwt;
					}
				}
			}

		}
	}
	
	// finally, normalize
	for (y=0; y<new->y; y++) {
		p = y * new->x;
		p4 = p * new->type;
		for (x=0; x<new->x; x++) {
			float dv = (wts->rect[p]==0.f) ? 1.f : (1.f/wts->rect[p]);
			new->rect[p4] *= dv;
			if (new->type!=CB_VAL) {
				new->rect[p4+1] *= dv;
				new->rect[p4+2] *= dv;
				new->rect[p4+3] *= dv;
			}
			p++;
			p4 += new->type;
		}
	}

	free_compbuf(crad);
	free_compbuf(wts);
	
	printf("Done\n");
}


static void node_composit_exec_defocus(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *old, *zbuf_use = NULL, *img = in[0]->data, *zbuf = in[1]->data;
	NodeDefocus* nqd = node->storage;
	
	if ((img==NULL) || (out[0]->hasoutput==0)) return;
	
	// if image not valid type or fstop==infinite (128), nothing to do, pass in to out
	if (((img->type!=CB_RGBA) && (img->type!=CB_VAL)) || ((nqd->no_zbuf==0) && (nqd->fstop==128.f))) {
		new = alloc_compbuf(img->x, img->y, img->type, 0);
		new->rect = img->rect;
		out[0]->data = new;
		return;
	}
	
	if (zbuf!=NULL) {
		// Zbuf input, check to make sure, single channel, same size
		// doesn't have to be actual zbuffer, but must be value type
		if ((zbuf->x != img->x) || (zbuf->y != img->y)) {
			// could do a scale here instead...
			printf("Z input must be same size as image !\n");
			return;
		}
		zbuf_use = typecheck_compbuf(zbuf, CB_VAL);
	}
	else nqd->no_zbuf = 1;	// no zbuffer input
		
	// ok, process
	old = img;
	if (nqd->gamco) {
		// gamma correct, blender func is simplified, fixed value & RGBA only, should make user param
		old = dupalloc_compbuf(img);
		gamma_correct_compbuf(old, 0);
	}
	
	new = alloc_compbuf(old->x, old->y, old->type, 1);
	defocus_blur(new, old, zbuf_use, in[1]->vec[0]*nqd->scale, node->storage);
	
	if (nqd->gamco) {
		gamma_correct_compbuf(new, 1);
		free_compbuf(old);
	}
	
	out[0]->data = new;
	if (zbuf_use && (zbuf_use != zbuf)) free_compbuf(zbuf_use);
}

static bNodeType cmp_node_defocus = {
	/* type code   */	CMP_NODE_DEFOCUS,
	/* name        */	"Defocus",
	/* width+range */	150, 120, 200,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_defocus_in,
	/* output sock */	cmp_node_defocus_out,
	/* storage     */	"NodeDefocus",
	/* execfunc    */	node_composit_exec_defocus
};

/* **************** VECTOR BLUR ******************** */
static bNodeSocketType cmp_node_vecblur_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Z",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Speed",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_vecblur_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};



static void node_composit_exec_vecblur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	NodeBlurData *nbd= node->storage;
	CompBuf *new, *img= in[0]->data, *vecbuf= in[2]->data, *zbuf= in[1]->data;
	
	if(img==NULL || vecbuf==NULL || zbuf==NULL || out[0]->hasoutput==0)
		return;
	if(vecbuf->x!=img->x || vecbuf->y!=img->y) {
		printf("ERROR: cannot do different sized vecbuf yet\n");
		return;
	}
	if(vecbuf->type!=CB_VEC4) {
		printf("ERROR: input should be vecbuf\n");
		return;
	}
	if(zbuf->type!=CB_VAL) {
		printf("ERROR: input should be zbuf\n");
		return;
	}
	if(zbuf->x!=img->x || zbuf->y!=img->y) {
		printf("ERROR: cannot do different sized zbuf yet\n");
		return;
	}
	
	/* allow the input image to be of another type */
	img= typecheck_compbuf(in[0]->data, CB_RGBA);

	new= dupalloc_compbuf(img);
	
	/* call special zbuffer version */
	RE_zbuf_accumulate_vecblur(nbd, img->x, img->y, new->rect, img->rect, vecbuf->rect, zbuf->rect);
	
	out[0]->data= new;
	
	if(img!=in[0]->data)
		free_compbuf(img);
}

/* custom1: itterations, custom2: maxspeed (0 = nolimit) */
static bNodeType cmp_node_vecblur= {
	/* type code   */	CMP_NODE_VECBLUR,
	/* name        */	"Vector Blur",
	/* width+range */	120, 80, 200,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_vecblur_in,
	/* output sock */	cmp_node_vecblur_out,
	/* storage     */	"NodeBlurData",
	/* execfunc    */	node_composit_exec_vecblur
	
};

/* **************** Translate  ******************** */

static bNodeSocketType cmp_node_translate_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "X",	0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	SOCK_VALUE, 1, "Y",	0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_translate_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_translate(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(in[0]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= pass_on_compbuf(cbuf);
	
		stackbuf->xof+= (int)floor(in[1]->vec[0]);
		stackbuf->yof+= (int)floor(in[2]->vec[0]);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_translate= {
	/* type code   */	CMP_NODE_TRANSLATE,
	/* name        */	"Translate",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_translate_in,
	/* output sock */	cmp_node_translate_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_translate
};

/* **************** Flip  ******************** */
static bNodeSocketType cmp_node_flip_in[]= {
	{	SOCK_RGBA, 1, "Image",		    0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType cmp_node_flip_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_flip(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(in[0]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 1);	/* note, this returns zero'd image */
		int i, src_pix, src_width, src_height, srcydelt, outydelt, x, y;
		float *srcfp, *outfp;
		
		src_pix= cbuf->type;
		src_width= cbuf->x;
		src_height= cbuf->y;
		srcfp= cbuf->rect;
		outfp= stackbuf->rect;
		srcydelt= src_width*src_pix;
		outydelt= srcydelt;
		
		if(node->custom1) {		/*set up output pointer for y flip*/
			outfp+= (src_height-1)*outydelt;
			outydelt= -outydelt;
		}

		for(y=0; y<src_height; y++) {
			if(node->custom1 == 1) {	/* no x flip so just copy line*/
				memcpy(outfp, srcfp, sizeof(float) * src_pix * src_width);
				srcfp+=srcydelt;
			}
			else {
				outfp += (src_width-1)*src_pix;
				for(x=0; x<src_width; x++) {
					for(i=0; i<src_pix; i++) {
						outfp[i]= srcfp[i];
					}
					outfp -= src_pix;
					srcfp += src_pix;
				}
				outfp += src_pix;
			}
			outfp += outydelt;
		}

		out[0]->data= stackbuf;

	}
}

static bNodeType cmp_node_flip= {
	/* type code   */	CMP_NODE_FLIP,
	/* name        */	"Flip",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_flip_in,
	/* output sock */	cmp_node_flip_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_flip
};

/* **************** Dilate/Erode ******************** */

static bNodeSocketType cmp_node_dilateerode_in[]= {
	{	SOCK_VALUE, 1, "Mask",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_dilateerode_out[]= {
	{	SOCK_VALUE, 0, "Mask",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void morpho_dilate(CompBuf *cbuf)
{
	int x, y;
	float *p, *rectf = cbuf->rect;
	
	for (y=0; y < cbuf->y; y++) {
		for (x=0; x < cbuf->x-1; x++) {
			p = rectf + cbuf->x*y + x;
			*p = MAX2(*p, *(p + 1));
		}
	}

	for (y=0; y < cbuf->y; y++) {
		for (x=cbuf->x-1; x >= 1; x--) {
			p = rectf + cbuf->x*y + x;
			*p = MAX2(*p, *(p - 1));
		}
	}

	for (x=0; x < cbuf->x; x++) {
		for (y=0; y < cbuf->y-1; y++) {
			p = rectf + cbuf->x*y + x;
			*p = MAX2(*p, *(p + cbuf->x));
		}
	}

	for (x=0; x < cbuf->x; x++) {
		for (y=cbuf->y-1; y >= 1; y--) {
			p = rectf + cbuf->x*y + x;
			*p = MAX2(*p, *(p - cbuf->x));
		}
	}
}

static void morpho_erode(CompBuf *cbuf)
{
	int x, y;
	float *p, *rectf = cbuf->rect;
	
	for (y=0; y < cbuf->y; y++) {
		for (x=0; x < cbuf->x-1; x++) {
			p = rectf + cbuf->x*y + x;
			*p = MIN2(*p, *(p + 1));
		}
	}

	for (y=0; y < cbuf->y; y++) {
		for (x=cbuf->x-1; x >= 1; x--) {
			p = rectf + cbuf->x*y + x;
			*p = MIN2(*p, *(p - 1));
		}
	}

	for (x=0; x < cbuf->x; x++) {
		for (y=0; y < cbuf->y-1; y++) {
			p = rectf + cbuf->x*y + x;
			*p = MIN2(*p, *(p + cbuf->x));
		}
	}

	for (x=0; x < cbuf->x; x++) {
		for (y=cbuf->y-1; y >= 1; y--) {
			p = rectf + cbuf->x*y + x;
			*p = MIN2(*p, *(p - cbuf->x));
		}
	}
	
}

static void node_composit_exec_dilateerode(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: mask */
	/* stack order out: mask */
	if(out[0]->hasoutput==0) 
		return;
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		out[0]->vec[0] = out[0]->vec[1] = out[0]->vec[2] = 0.0f;
		out[0]->vec[3] = 0.0f;
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_VAL);
		CompBuf *stackbuf= dupalloc_compbuf(cbuf);
		short i;
		
		if (node->custom2 > 0) { // positive, dilate
			for (i = 0; i < node->custom2; i++)
				morpho_dilate(stackbuf);
		} else if (node->custom2 < 0) { // negative, erode
			for (i = 0; i > node->custom2; i--)
				morpho_erode(stackbuf);
		}
		
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_dilateerode= {
	/* type code   */	CMP_NODE_DILATEERODE,
	/* name        */	"Dilate/Erode",
	/* width+range */	130, 100, 320,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_dilateerode_in,
	/* output sock */	cmp_node_dilateerode_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_dilateerode
};


/* **************** SEPARATE YUVA ******************** */
static bNodeSocketType cmp_node_sepyuva_in[]= {
	{  SOCK_RGBA, 1, "Image",        0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{  -1, 0, ""   }
};
static bNodeSocketType cmp_node_sepyuva_out[]= {
	{  SOCK_VALUE, 0, "Y",        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  SOCK_VALUE, 0, "U",        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  SOCK_VALUE, 0, "V",        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  SOCK_VALUE, 0, "A",        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  -1, 0, ""   }
};

static void do_sepyuva(bNode *node, float *out, float *in)
{
	float y, u, v;
	
	rgb_to_yuv(in[0], in[1], in[2], &y, &u, &v);
	
	out[0]= y;
	out[1]= u;
	out[2]= v;
	out[3]= in[3];
}

static void node_composit_exec_sepyuva(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw channels */
	/* stack order in: col */
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		float y, u, v;
	
		rgb_to_yuv(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &y, &u, &v);
	
		out[0]->vec[0] = y;
		out[1]->vec[0] = u;
		out[2]->vec[0] = v;
		out[3]->vec[0] = in[0]->vec[3];
	}
	else if ((out[0]->hasoutput) || (out[1]->hasoutput) || (out[2]->hasoutput) || (out[3]->hasoutput)) {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
	
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
	
		/* convert the RGB stackbuf to an YUV representation */
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_sepyuva, CB_RGBA);
	
		/* separate each of those channels */
		if(out[0]->hasoutput)
			out[0]->data= valbuf_from_rgbabuf(stackbuf, CHAN_R);
		if(out[1]->hasoutput)
			out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_G);
		if(out[2]->hasoutput)
			out[2]->data= valbuf_from_rgbabuf(stackbuf, CHAN_B);
		if(out[3]->hasoutput)
			out[3]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);
	
		free_compbuf(stackbuf);
	}
}

static bNodeType cmp_node_sepyuva= {
	/* type code   */ CMP_NODE_SEPYUVA,
	/* name        */ "Separate YUVA",
	/* width+range */ 80, 40, 140,
	/* class+opts  */ NODE_CLASS_CONVERTOR, 0,
	/* input sock  */ cmp_node_sepyuva_in,
	/* output sock */ cmp_node_sepyuva_out,
	/* storage     */ "",
	/* execfunc    */ node_composit_exec_sepyuva
};

/* **************** SEPARATE YCCA ******************** */
static bNodeSocketType cmp_node_sepycca_in[]= {
	{  SOCK_RGBA, 1, "Image",        0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{  -1, 0, ""   }
};
static bNodeSocketType cmp_node_sepycca_out[]= {
	{  SOCK_VALUE, 0, "Y",        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  SOCK_VALUE, 0, "Cb",       0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  SOCK_VALUE, 0, "Cr",       0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  SOCK_VALUE, 0, "A",        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{  -1, 0, ""   }
};

static void do_sepycca(bNode *node, float *out, float *in)
{
	float y, cb, cr;
	
	rgb_to_ycc(in[0], in[1], in[2], &y, &cb, &cr);
	
	/*divided by 255 to normalize for viewing in */
	out[0]= y/255.0;
	out[1]= cb/255.0;
	out[2]= cr/255.0;
	out[3]= in[3];
}

static void node_composit_exec_sepycca(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		float y, cb, cr;
	
		rgb_to_ycc(in[0]->vec[0], in[0]->vec[1], in[0]->vec[2], &y, &cb, &cr);
	
		/*divided by 255 to normalize for viewing in */
		out[0]->vec[0] = y/255.0;
		out[1]->vec[0] = cb/255.0;
		out[2]->vec[0] = cr/255.0;
		out[3]->vec[0] = in[0]->vec[3];
	}
	else if ((out[0]->hasoutput) || (out[1]->hasoutput) || (out[2]->hasoutput) || (out[3]->hasoutput)) {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
	
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
	
		/* convert the RGB stackbuf to an HSV representation */
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_sepycca, CB_RGBA);
	
		/* separate each of those channels */
		if(out[0]->hasoutput)
			out[0]->data= valbuf_from_rgbabuf(stackbuf, CHAN_R);
		if(out[1]->hasoutput)
			out[1]->data= valbuf_from_rgbabuf(stackbuf, CHAN_G);
		if(out[2]->hasoutput)
			out[2]->data= valbuf_from_rgbabuf(stackbuf, CHAN_B);
		if(out[3]->hasoutput)
			out[3]->data= valbuf_from_rgbabuf(stackbuf, CHAN_A);
	
		free_compbuf(stackbuf);
	}
}

static bNodeType cmp_node_sepycca= {
	/* type code   */ CMP_NODE_SEPYCCA,
	/* name        */ "Separate YCbCrA",
	/* width+range */ 80, 40, 140,
	/* class+opts  */ NODE_CLASS_CONVERTOR, 0,
	/* input sock  */ cmp_node_sepycca_in,
	/* output sock */ cmp_node_sepycca_out,
	/* storage     */ "",
	/* execfunc    */ node_composit_exec_sepycca
};


/* ******************* channel Difference Matte ********************************* */
static bNodeSocketType cmp_node_diff_matte_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_RGBA,1,"Key Color", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_diff_matte_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_rgba_to_yuva(bNode *node, float *out, float *in)
{
	rgb_to_yuv(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
			out[3]=in[3];
}

static void do_rgba_to_hsva(bNode *node, float *out, float *in)
{
	rgb_to_hsv(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
			out[3]=in[3];
}

static void do_rgba_to_ycca(bNode *node, float *out, float *in)
{
	rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
			out[3]=in[3];
}

static void do_yuva_to_rgba(bNode *node, float *out, float *in)
{
	yuv_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
			out[3]=in[3];
}

static void do_hsva_to_rgba(bNode *node, float *out, float *in)
{
	hsv_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
			out[3]=in[3];
}

static void do_ycca_to_rgba(bNode *node, float *out, float *in)
{
	ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
			out[3]=in[3];
}

/* note, keyvals is passed on from caller as stack array */
/* might have been nicer as temp struct though... */
static void do_diff_matte(bNode *node, float *colorbuf, float *inbuf, float *keyvals)
{
	NodeChroma *c= (NodeChroma *)node->storage;
	float *keymin= keyvals;
	float *keymax= keyvals+3;
	float *key=    keyvals+6;
	float tolerance= keyvals[9];
	float distance, alpha;
	
	/*process the pixel if it is close to the key or already transparent*/
	if(((colorbuf[0]>keymin[0] && colorbuf[0]<keymax[0]) &&
		(colorbuf[1]>keymin[1] && colorbuf[1]<keymax[1]) &&
		(colorbuf[2]>keymin[2] && colorbuf[2]<keymax[2])) || inbuf[3]<1.0f) {
		
		/*true distance from key*/
		distance= sqrt((colorbuf[0]-key[0])*(colorbuf[0]-key[0])+
					  (colorbuf[1]-key[1])*(colorbuf[1]-key[1])+
					  (colorbuf[2]-key[2])*(colorbuf[2]-key[2]));
		
		/*is it less transparent than the prevous pixel*/
		alpha= distance/tolerance;
		if(alpha > inbuf[3]) alpha= inbuf[3];
		if(alpha > c->fstrength) alpha= 0.0f;
		
		/*clamp*/
		if (alpha>1.0f) alpha=1.0f;
		if (alpha<0.0f) alpha=0.0f;
		
		/*premultiplied picture*/
		colorbuf[3]= alpha;
	}
	else {
		/*foreground object*/
		colorbuf[3]= inbuf[3];
	}
}

static void node_composit_exec_diff_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/*
	Losely based on the Sequencer chroma key plug-in, but enhanced to work in other color spaces and
	uses a differnt difference function (suggested in forums of vfxtalk.com).
	*/
	CompBuf *outbuf;
	CompBuf *colorbuf;
	CompBuf *inbuf;
	NodeChroma *c;
	float keyvals[10];
	float *keymin= keyvals;
	float *keymax= keyvals+3;
	float *key=    keyvals+6;
	float *tolerance= keyvals+9;
	float t[3];
	
	/*is anything connected?*/
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	/*must have an image imput*/
	if(in[0]->data==NULL) return;
	
	inbuf=in[0]->data;
	if(inbuf->type!=CB_RGBA) return;
	
	c=node->storage;
	colorbuf=dupalloc_compbuf(inbuf);
	outbuf=alloc_compbuf(inbuf->x,inbuf->y,CB_RGBA,1);
	
	/*use the input color*/
	key[0]= in[1]->vec[0];
	key[1]= in[1]->vec[1];
	key[2]= in[1]->vec[2];
	
	/*get the tolerances from the UI*/
	t[0]=c->t1;
	t[1]=c->t2;
	t[2]=c->t3;
	
	/*convert to colorspace*/
	switch(node->custom1) {
		case 1: /*RGB*/
				break;
		case 2: /*HSV*/
		/*convert the key (in place)*/
			rgb_to_hsv(key[0], key[1], key[2], &key[0], &key[1], &key[2]);
			composit1_pixel_processor(node, colorbuf, inbuf, in[1]->vec, do_rgba_to_hsva, CB_RGBA);
			break;
		case 3: /*YUV*/
			rgb_to_yuv(key[0], key[1], key[2], &key[0], &key[1], &key[2]);
			composit1_pixel_processor(node, colorbuf, inbuf, in[1]->vec, do_rgba_to_yuva, CB_RGBA);
			break;
		case 4: /*YCC*/
			rgb_to_ycc(key[0], key[1], key[2], &key[0], &key[1], &key[2]);
			composit1_pixel_processor(node, colorbuf, inbuf, in[1]->vec, do_rgba_to_ycca, CB_RGBA);
			/*account for ycc is on a 0..255 scale*/
			t[0]= c->t1*255.0;
			t[1]= c->t2*255.0;
			t[2]= c->t3*255.0;
			break;
		default:
				break;
	}
	
	/*find min/max tolerances*/
	keymin[0]= key[0]-t[0];
	keymin[1]= key[1]-t[1];
	keymin[2]= key[2]-t[2];
	keymax[0]= key[0]+t[0];
	keymax[1]= key[1]+t[1];
	keymax[2]= key[2]+t[2];
	
	/*tolerance*/
	*tolerance= sqrt((t[0])*(t[0])+
					(t[1])*(t[1])+
					(t[2])*(t[2]));
	
	/* note, processor gets a keyvals array passed on as buffer constant */
	composit2_pixel_processor(node, colorbuf, inbuf, in[0]->vec, NULL, keyvals, do_diff_matte, CB_RGBA, CB_VAL);
	
	/*convert back to RGB colorspace*/
	switch(node->custom1) {
		case 1: /*RGB*/
			composit1_pixel_processor(node, outbuf, colorbuf, in[1]->vec, do_copy_rgba, CB_RGBA);
			break;
		case 2: /*HSV*/
			composit1_pixel_processor(node, outbuf, colorbuf, in[1]->vec, do_hsva_to_rgba, CB_RGBA);
			break;
		case 3: /*YUV*/
			composit1_pixel_processor(node, outbuf, colorbuf, in[1]->vec, do_yuva_to_rgba, CB_RGBA);
			break;
		case 4: /*YCC*/
			composit1_pixel_processor(node, outbuf, colorbuf, in[1]->vec, do_ycca_to_rgba, CB_RGBA);
			break;
		default:
			break;
	}
	
	free_compbuf(colorbuf);
	
	out[0]->data=outbuf;
	out[1]->data=valbuf_from_rgbabuf(outbuf, CHAN_A);
	generate_preview(node, outbuf);
}

static bNodeType cmp_node_diff_matte={
	/* type code   */       CMP_NODE_DIFF_MATTE,
	/* name        */       "Difference Key",
	/* width+range */       200, 80, 250,
	/* class+opts  */       NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */       cmp_node_diff_matte_in,
	/* output sock */       cmp_node_diff_matte_out,
	/* storage     */       "NodeChroma",
	/* execfunc    */       node_composit_exec_diff_matte
};


/* ******************* Color Spill Supression ********************************* */
static bNodeSocketType cmp_node_color_spill_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_color_spill_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_reduce_red(bNode *node, float* out, float *in)
{
	NodeChroma *c;
	c=node->storage;
	
	if(in[0] > in[1] && in[0] > in[2]) {
		out[0]=((in[1]+in[2])/2)*(1-c->t1);
	}
}

static void do_reduce_green(bNode *node, float* out, float *in)
{
	NodeChroma *c;
	c=node->storage;
	
	if(in[1] > in[0] && in[1] > in[2]) {
		out[1]=((in[0]+in[2])/2)*(1-c->t1);
	}
}

static void do_reduce_blue(bNode *node, float* out, float *in)
{
	NodeChroma *c;
	c=node->storage;
	
	if(in[2] > in[1] && in[2] > in[1]) {
		out[2]=((in[1]+in[0])/2)*(1-c->t1);
	}
}

static void node_composit_exec_color_spill(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/*
	Originally based on the information from the book "The Art and Science of Digital Composition" and 
	discussions from vfxtalk.com.*/
	CompBuf *cbuf;
	CompBuf *rgbbuf;
	
	if(out[0]->hasoutput==0 || in[0]->hasinput==0) return;
	if(in[0]->data==NULL) return;
	
	cbuf=in[0]->data;
	/*is it an RGBA image?*/
	if(cbuf->type==CB_RGBA) {
		
		rgbbuf=dupalloc_compbuf(cbuf);
		switch(node->custom1)
		{
		case 1:  /*red spill*/
			{
				composit1_pixel_processor(node, rgbbuf, cbuf, in[1]->vec, do_reduce_red, CB_RGBA);
				break;
			}
		case 2: /*green spill*/
			{
				composit1_pixel_processor(node, rgbbuf, cbuf, in[1]->vec, do_reduce_green, CB_RGBA);
				break;
			}
		case 3: /*blue spill*/
			{
				composit1_pixel_processor(node, rgbbuf, cbuf, in[1]->vec, do_reduce_blue, CB_RGBA);
				break;
			}
		default:
			break;
		}
	
		out[0]->data=rgbbuf;
	}
	else {
		return;
	}
}

static bNodeType cmp_node_color_spill={
	/* type code   */       CMP_NODE_COLOR_SPILL,
	/* name        */       "Color Spill",
	/* width+range */       140, 80, 200,
	/* class+opts  */       NODE_CLASS_MATTE, NODE_OPTIONS,
	/* input sock  */       cmp_node_color_spill_in,
	/* output sock */       cmp_node_color_spill_out,
	/* storage     */       "NodeChroma",
	/* execfunc    */       node_composit_exec_color_spill
};

/* ******************* Chroma Key ********************************************************** */
static bNodeSocketType cmp_node_chroma_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_chroma_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_rgba_to_ycca_normalized(bNode *node, float *out, float *in)
{
	rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[0]=(out[0])/255;
	out[1]=(out[1])/256;
	out[2]=(out[2])/256;
	out[3]=in[3];
}

static void do_normalized_ycca_to_rgba(bNode *node, float *out, float *in)
{
	in[0]=in[0]*255;  
	in[1]=in[1]*256;
	in[2]=in[2]*256;
	ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[3]=in[3];
}

static void do_chroma_key(bNode *node, float *out, float *in)
{
	/* Algorithm of my own design-Bob Holcomb */

	NodeChroma *c;
	float x, z, alpha;

	c=node->storage;
	switch(node->custom1)
	{
		case 1:  /*green*/
		{
			x=(atanf((c->t1*in[1])-(c->t1*c->t2))+1)/2;
			z=(atanf((c->t3*in[2])-(c->t3*c->fsize))+1)/2;
			break;
		}
		case 2:  /*blue*/
		{	
			x=(atanf((c->t1*in[1])-(c->t1*c->t2))+1)/2;
			z=(atanf((c->t3*in[2])-(c->t3*c->fsize))+1)/2;
			x=1-x;
			break;
		}
		default:
		{
			x= z= 0.0f;
			break;
		}
	}

	/*clamp to zero so that negative values don' affect the other channels input */
	if(x<0.0) x=0.0;
	if(z<0.0) z=0.0;

	/*if chroma values added are less than strength then it is a key value */
	if((x+z) < c->fstrength) {
		alpha=(x+z);
		alpha=in[0]+alpha; /*add in the luminence for detail */
		if(alpha > c->falpha) alpha=0;  /* if below the threshold */
		if(alpha < in[3]) { /* is it less than the previous alpha */
			out[3]=alpha;
		}
		else {
			out[3]=in[3];
		}
	}
	
}

static void node_composit_exec_chroma(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *rgbbuf,*inbuf;
	CompBuf *chromabuf;
	NodeChroma *c;
	
	if(out[0]->hasoutput==0 || in[0]->hasinput==0) return;
	if(in[0]->data==NULL) return;
	
	inbuf= in[0]->data;
	if(inbuf->type!=CB_RGBA) return;
	
	rgbbuf= dupalloc_compbuf(inbuf);
	chromabuf= dupalloc_compbuf(rgbbuf);
	
	c=node->storage;
	
	/*convert rgbbuf to normalized chroma space*/
	composit1_pixel_processor(node, chromabuf, inbuf, in[0]->vec, do_rgba_to_ycca_normalized, CB_RGBA);
	
	/*per pixel chroma key*/
	composit1_pixel_processor(node, chromabuf, chromabuf, in[0]->vec, do_chroma_key, CB_RGBA);
	
	/*convert back*/
	composit1_pixel_processor(node, rgbbuf, chromabuf, in[0]->vec, do_normalized_ycca_to_rgba, CB_RGBA);
	
	/*cleanup */
	free_compbuf(chromabuf);
	
	out[0]->data= rgbbuf;
	if(out[1]->hasoutput)
		out[1]->data= valbuf_from_rgbabuf(rgbbuf, CHAN_A);
	
	generate_preview(node, rgbbuf);
};

static bNodeType cmp_node_chroma={
	/* type code   */       CMP_NODE_CHROMA,
	/* name        */       "Chroma Key",
	/* width+range */       200, 80, 300,
	/* class+opts  */       NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */       cmp_node_chroma_in,
	/* output sock */       cmp_node_chroma_out,
	/* storage     */       "NodeChroma",
	/* execfunc    */       node_composit_exec_chroma
};

/* ******************* Channel Matte Node ********************************* */
static bNodeSocketType cmp_node_channel_matte_in[]={
	{SOCK_RGBA,1,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static bNodeSocketType cmp_node_channel_matte_out[]={
	{SOCK_RGBA,0,"Image", 0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{SOCK_VALUE,0,"Matte",0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{-1,0,""}
};

static void do_channel_matte(bNode *node, float *out, float *in)
{
	NodeChroma *c=(NodeChroma *)node->storage;
	float alpha=0.0;
	
	/* Alpha=G-MAX(R, B) */
	
	switch(node->custom2)
	{
	case 1:
		{
			alpha=in[0]-MAX2(in[1],in[2]);
			break;
		}
	case 2:
		{
			alpha=in[1]-MAX2(in[0],in[2]);
			break;
		}
	case 3:
		{
			alpha=in[2]-MAX2(in[0],in[1]);
			break;
		}
	default:
		break;
	}
	
	/*flip because 0.0 is transparent, not 1.0*/
	alpha=1-alpha;
	
	//test range
	if(alpha>c->t1) {
		alpha=in[3]; /*whatever it was prior */
	}
	else if(alpha<c->t2){
		alpha=0.0;
	}
	else {/*blend */
		alpha=(alpha-c->t2)/(c->t1-c->t2);
	}
	
	/* don't make something that was more transparent less transparent */
	if (alpha<in[3]) {
		out[3]=alpha;
	}
	else {
		out[3]=in[3];
	}

}

static void node_composit_exec_channel_matte(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *cbuf;
	CompBuf *outbuf;
	
	if(in[0]->hasinput==0)  return;
	if(in[0]->data==NULL) return;
	if(out[0]->hasoutput==0 && out[1]->hasoutput==0) return;
	
	cbuf=in[0]->data;
	/*is it an RGBA image?*/
	if(cbuf->type==CB_RGBA) {
	
		outbuf=dupalloc_compbuf(cbuf);
	
		/*convert to colorspace*/
		switch(node->custom1) {
		case 1: /*RGB */
			break;
		case 2: /*HSV*/
			composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_rgba_to_hsva, CB_RGBA);
			break;
		case 3: /*YUV*/
			composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_rgba_to_yuva, CB_RGBA);
			break;
		case 4: /*YCC*/
			composit1_pixel_processor(node, outbuf, cbuf, in[1]->vec, do_rgba_to_ycca, CB_RGBA);
			break;
		default:
			break;
		}
	
		/*use the selected channel information to do the key */
		composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_channel_matte, CB_RGBA);
	
		/*convert back to RGB colorspace in place*/
		switch(node->custom1) {
		case 1: /*RGB*/
			break;
		case 2: /*HSV*/
			composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_hsva_to_rgba, CB_RGBA);
			break;
		case 3: /*YUV*/
			composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_yuva_to_rgba, CB_RGBA);
			break;
		case 4: /*YCC*/
			composit1_pixel_processor(node, outbuf, outbuf, in[1]->vec, do_ycca_to_rgba, CB_RGBA);
			break;
		default:
			break;
		}
	
		generate_preview(node, outbuf);
		out[0]->data=outbuf;
		out[1]->data=valbuf_from_rgbabuf(outbuf, CHAN_A);
	
	}
	else {
		return;
	}
}

static bNodeType cmp_node_channel_matte={
	/* type code   */       CMP_NODE_CHANNEL_MATTE,
	/* name        */       "Channel Key",
	/* width+range */       200, 80, 250,
	/* class+opts  */       NODE_CLASS_MATTE, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */       cmp_node_channel_matte_in,
	/* output sock */       cmp_node_channel_matte_out,
	/* storage     */       "NodeChroma",
	/* execfunc    */       node_composit_exec_channel_matte
};



/* **************** COMBINE YCCA ******************** */
static bNodeSocketType cmp_node_combycca_in[]= {
	{	SOCK_VALUE, 1, "Y",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Cb",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Cr",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "A",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_combycca_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_comb_ycca(bNode *node, float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r,g,b;
	float y, cb, cr;

	/*need to un-normalize the data*/
	y=in1[0]*255;
	cb=in2[0]*255;
	cr=in3[0]*255;

	ycc_to_rgb(y,cb,cr, &r, &g, &b);
	
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void node_composit_exec_combycca(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: 1 ycca channels */
	/* stack order in: 4 value channels */
	
	/* input no image? then only color operation */
	if((in[0]->data==NULL) && (in[1]->data==NULL) && (in[2]->data==NULL) && (in[3]->data==NULL)) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[1]->vec[0];
		out[0]->vec[2] = in[2]->vec[0];
		out[0]->vec[3] = in[3]->vec[0];
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf;
		CompBuf *stackbuf;

		/* allocate a CompBuf the size of the first available input */
		if (in[0]->data) cbuf = in[0]->data;
		else if (in[1]->data) cbuf = in[1]->data;
		else if (in[2]->data) cbuf = in[2]->data;
		else cbuf = in[3]->data;
		
		stackbuf = alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
								  in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
								  do_comb_ycca, CB_VAL, CB_VAL, CB_VAL, CB_VAL);

		out[0]->data= stackbuf;
	}	
}

static bNodeType cmp_node_combycca= {
	/* type code   */	CMP_NODE_COMBYCCA,
	/* name        */	"Combine YCbCrA",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_combycca_in,
	/* output sock */	cmp_node_combycca_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_combycca
};


/* **************** COMBINE YUVA ******************** */
static bNodeSocketType cmp_node_combyuva_in[]= {
	{	SOCK_VALUE, 1, "Y",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "U",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "V",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "A",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_combyuva_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_comb_yuva(bNode *node, float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r,g,b;
	yuv_to_rgb(in1[0], in2[0], in3[0], &r, &g, &b);
	
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void node_composit_exec_combyuva(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: 1 rgba channels */
	/* stack order in: 4 value channels */
	
	/* input no image? then only color operation */
	if((in[0]->data==NULL) && (in[1]->data==NULL) && (in[2]->data==NULL) && (in[3]->data==NULL)) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[1]->vec[0];
		out[0]->vec[2] = in[2]->vec[0];
		out[0]->vec[3] = in[3]->vec[0];
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf;
		CompBuf *stackbuf;

		/* allocate a CompBuf the size of the first available input */
		if (in[0]->data) cbuf = in[0]->data;
		else if (in[1]->data) cbuf = in[1]->data;
		else if (in[2]->data) cbuf = in[2]->data;
		else cbuf = in[3]->data;
		
		stackbuf = alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
								  in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
								  do_comb_yuva, CB_VAL, CB_VAL, CB_VAL, CB_VAL);

		out[0]->data= stackbuf;
	}	
}

static bNodeType cmp_node_combyuva= {
	/* type code   */	CMP_NODE_COMBYUVA,
	/* name        */	"Combine YUVA",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_combyuva_in,
	/* output sock */	cmp_node_combyuva_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_combyuva
};

/* **************** COMBINE HSVA ******************** */
static bNodeSocketType cmp_node_combhsva_in[]= {
	{	SOCK_VALUE, 1, "H",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "S",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "V",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "A",			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_combhsva_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_comb_hsva(bNode *node, float *out, float *in1, float *in2, float *in3, float *in4)
{
	float r,g,b;
	hsv_to_rgb(in1[0], in2[0], in3[0], &r, &g, &b);
	
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = in4[0];
}

static void node_composit_exec_combhsva(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: 1 rgba channels */
	/* stack order in: 4 value channels */
	
	/* input no image? then only color operation */
	if((in[0]->data==NULL) && (in[1]->data==NULL) && (in[2]->data==NULL) && (in[3]->data==NULL)) {
		out[0]->vec[0] = in[0]->vec[0];
		out[0]->vec[1] = in[1]->vec[0];
		out[0]->vec[2] = in[2]->vec[0];
		out[0]->vec[3] = in[3]->vec[0];
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf;
		CompBuf *stackbuf;

		/* allocate a CompBuf the size of the first available input */
		if (in[0]->data) cbuf = in[0]->data;
		else if (in[1]->data) cbuf = in[1]->data;
		else if (in[2]->data) cbuf = in[2]->data;
		else cbuf = in[3]->data;
		
		stackbuf = alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit4_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, 
								  in[2]->data, in[2]->vec, in[3]->data, in[3]->vec, 
								  do_comb_hsva, CB_VAL, CB_VAL, CB_VAL, CB_VAL);

		out[0]->data= stackbuf;
	}	
}

static bNodeType cmp_node_combhsva= {
	/* type code   */	CMP_NODE_COMBHSVA,
	/* name        */	"Combine HSVA",
	/* width+range */	80, 40, 140,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_combhsva_in,
	/* output sock */	cmp_node_combhsva_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_combhsva
};



/* **************** Rotate  ******************** */

static bNodeSocketType cmp_node_rotate_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Degr",			0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_rotate_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* function assumes out to be zero'ed, only does RGBA */
static void bilinear_interpolation_rotate(CompBuf *in, float *out, float u, float v)
{
	float *row1, *row2, *row3, *row4, a, b;
	float a_b, ma_b, a_mb, ma_mb;
	float empty[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	int y1, y2, x1, x2;

	x1= (int)floor(u);
	x2= (int)ceil(u);
	y1= (int)floor(v);
	y2= (int)ceil(v);

	/* sample area entirely outside image? */
	if(x2<0 || x1>in->x-1 || y2<0 || y1>in->y-1)
		return;
	
	/* sample including outside of edges of image */
	if(x1<0 || y1<0) row1= empty;
	else row1= in->rect + in->x * y1 * in->type + in->type*x1;
	
	if(x1<0 || y2>in->y-1) row2= empty;
	else row2= in->rect + in->x * y2 * in->type + in->type*x1;
	
	if(x2>in->x-1 || y1<0) row3= empty;
	else row3= in->rect + in->x * y1 * in->type + in->type*x2;
	
	if(x2>in->x-1 || y2>in->y-1) row4= empty;
	else row4= in->rect + in->x * y2 * in->type + in->type*x2;
	
	a= u-floor(u);
	b= v-floor(v);
	a_b= a*b; ma_b= (1.0f-a)*b; a_mb= a*(1.0f-b); ma_mb= (1.0f-a)*(1.0f-b);
	
	out[0]= ma_mb*row1[0] + a_mb*row3[0] + ma_b*row2[0]+ a_b*row4[0];
	out[1]= ma_mb*row1[1] + a_mb*row3[1] + ma_b*row2[1]+ a_b*row4[1];
	out[2]= ma_mb*row1[2] + a_mb*row3[2] + ma_b*row2[2]+ a_b*row4[2];
	out[3]= ma_mb*row1[3] + a_mb*row3[3] + ma_b*row2[3]+ a_b*row4[3];
}

/* only supports RGBA nodes now */
static void node_composit_exec_rotate(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1);	/* note, this returns zero'd image */
		float *ofp, rad, u, v, s, c, centx, centy, miny, maxy, minx, maxx;
		int x, y, yo;
	
		rad= (M_PI*in[1]->vec[0])/180.0f;
		
		s= sin(rad);
		c= cos(rad);
		centx= cbuf->x/2;
		centy= cbuf->y/2;
		
		minx= -centx;
		maxx= -centx + (float)cbuf->x;
		miny= -centy;
		maxy= -centy + (float)cbuf->y;
		
		for(y=miny; y<maxy; y++) {
			yo= y+(int)centy;
			ofp= stackbuf->rect + 4*yo*stackbuf->x;
			
			for(x=minx; x<maxx; x++, ofp+=4) {
				u= c*x + y*s + centx;
				v= -s*x + c*y + centy;
				
				bilinear_interpolation_rotate(cbuf, ofp, u, v);
			}
		}
		/* rotate offset vector too, but why negative rad, ehh?? Has to be replaced with [3][3] matrix once (ton) */
		s= sin(-rad);
		c= cos(-rad);
		centx= (float)cbuf->xof; centy= (float)cbuf->yof;
		stackbuf->xof= (int)( c*centx + s*centy);
		stackbuf->yof= (int)(-s*centx + c*centy);
		
		/* pass on output and free */
		out[0]->data= stackbuf;
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
		
	}
}

static bNodeType cmp_node_rotate= {
	/* type code   */	CMP_NODE_ROTATE,
	/* name        */	"Rotate",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_rotate_in,
	/* output sock */	cmp_node_rotate_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rotate
};


/* **************** Scale  ******************** */

#define CMP_SCALE_MAX	12000

static bNodeSocketType cmp_node_scale_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "X",				1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX},
	{	SOCK_VALUE, 1, "Y",				1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_scale_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* only supports RGBA nodes now */
/* node->custom1 stores if input values are absolute or relative scale */
static void node_composit_exec_scale(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *stackbuf, *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		ImBuf *ibuf;
		int newx, newy;
		
		if(node->custom1==CMP_SCALE_RELATIVE) {
			newx= MAX2((int)(in[1]->vec[0]*cbuf->x), 1);
			newy= MAX2((int)(in[2]->vec[0]*cbuf->y), 1);
		}
		else {	/* CMP_SCALE_ABSOLUTE */
			newx= (int)in[1]->vec[0];
			newy= (int)in[2]->vec[0];
		}
		newx= MIN2(newx, CMP_SCALE_MAX);
		newy= MIN2(newy, CMP_SCALE_MAX);

		ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
		if(ibuf) {
			ibuf->rect_float= cbuf->rect;
			IMB_scaleImBuf(ibuf, newx, newy);
			
			if(ibuf->rect_float == cbuf->rect) {
				/* no scaling happened. */
				stackbuf= pass_on_compbuf(in[0]->data);
			}
			else {
				stackbuf= alloc_compbuf(newx, newy, CB_RGBA, 0);
				stackbuf->rect= ibuf->rect_float;
				stackbuf->malloc= 1;
			}

			ibuf->rect_float= NULL;
			ibuf->mall &= ~IB_rectfloat;
			IMB_freeImBuf(ibuf);
			
			/* also do the translation vector */
			stackbuf->xof = (int)(((float)newx/(float)cbuf->x) * (float)cbuf->xof);
			stackbuf->yof = (int)(((float)newy/(float)cbuf->y) * (float)cbuf->yof);
		}
		else {
			stackbuf= dupalloc_compbuf(cbuf);
			printf("Scaling to %dx%d failed\n", newx, newy);
		}
		
		out[0]->data= stackbuf;
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
	}
}

static bNodeType cmp_node_scale= {
	/* type code   */	CMP_NODE_SCALE,
	/* name        */	"Scale",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_scale_in,
	/* output sock */	cmp_node_scale_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_scale
};

/* **************** Map UV  ******************** */

static bNodeSocketType cmp_node_mapuv_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "UV",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_mapuv_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* foreach UV, use these values to read in cbuf and write to stackbuf */
/* stackbuf should be zeroed */
static void do_mapuv(CompBuf *stackbuf, CompBuf *cbuf, CompBuf *uvbuf, float threshold)
{
	ImBuf *ibuf;
	float *out= stackbuf->rect, *uv, *uvnext, *uvprev;
	float dx, dy, alpha;
	int x, y, sx, sy, row= 3*stackbuf->x;
	
	/* ibuf needed for sampling */
	ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
	ibuf->rect_float= cbuf->rect;
	
	/* vars for efficient looping */
	uv= uvbuf->rect;
	uvnext= uv+row;
	uvprev= uv-row;
	sx= stackbuf->x;
	sy= stackbuf->y;
	
	for(y=0; y<sy; y++) {
		for(x=0; x<sx; x++, out+=4, uv+=3, uvnext+=3, uvprev+=3) {
			if(x>0 && x<sx-1 && y>0 && y<sy-1) {
				if(uv[2]!=0.0f) {
					
					/* adaptive sampling, red (U) channel */
					dx= 0.5f*(fabs(uv[0]-uv[-3]) + fabs(uv[0]-uv[3]));
					
					dx+= 0.25f*(fabs(uv[0]-uvprev[-3]) + fabs(uv[0]-uvnext[-3]));
					dx+= 0.25f*(fabs(uv[0]-uvprev[+3]) + fabs(uv[0]-uvnext[+3]));
					
					/* adaptive sampling, green (V) channel */
					dy= 0.5f*(fabs(uv[1]-uv[-row+1]) + fabs(uv[1]-uv[row+1]));
							 
					dy+= 0.25f*(fabs(uv[1]-uvprev[+1-3]) + fabs(uv[1]-uvnext[+1-3]));
					dy+= 0.25f*(fabs(uv[1]-uvprev[+1+3]) + fabs(uv[1]-uvnext[+1+3]));
					
					/* UV to alpha threshold */
					alpha= 1.0f - threshold*(dx+dy);
					if(alpha<0.0f) alpha= 0.0f;
					else alpha*= uv[2];
					
					/* should use mipmap */
					if(dx > 0.20f) dx= 0.20f;
					if(dy > 0.20f) dy= 0.20f;
					
					ibuf_sample(ibuf, uv[0], uv[1], dx, dy, out);
					/* premul */
					if(alpha<1.0f) {
						out[0]*= alpha;
						out[1]*= alpha;
						out[2]*= alpha;
						out[3]*= alpha;
					}
				}
			}
		}
	}

	IMB_freeImBuf(ibuf);	
}


static void node_composit_exec_mapuv(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data && in[1]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *uvbuf= in[1]->data;
		CompBuf *stackbuf;
		
		cbuf= typecheck_compbuf(cbuf, CB_RGBA);
		uvbuf= typecheck_compbuf(uvbuf, CB_VEC3);
		stackbuf= alloc_compbuf(uvbuf->x, uvbuf->y, CB_RGBA, 1); // allocs;
		
		do_mapuv(stackbuf, cbuf, uvbuf, 0.05f*(float)node->custom1);
		
		out[0]->data= stackbuf;
		
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
		if(uvbuf!=in[1]->data)
			free_compbuf(uvbuf);
	}
}

static bNodeType cmp_node_mapuv= {
	/* type code   */	CMP_NODE_MAP_UV,
	/* name        */	"Map UV",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_mapuv_in,
	/* output sock */	cmp_node_mapuv_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_mapuv
};


/* **************** ID Mask  ******************** */

static bNodeSocketType cmp_node_idmask_in[]= {
	{	SOCK_VALUE, 1, "ID value",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_idmask_out[]= {
	{	SOCK_VALUE, 0, "Alpha",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* stackbuf should be zeroed */
static void do_idmask(CompBuf *stackbuf, CompBuf *cbuf, float idnr)
{
	float *rect;
	int x;
	char *abuf= MEM_mapallocN(cbuf->x*cbuf->y, "anti ali buf");
	
	rect= cbuf->rect;
	for(x= cbuf->x*cbuf->y - 1; x>=0; x--)
		if(rect[x]==idnr)
			abuf[x]= 255;
	
	antialias_tagbuf(cbuf->x, cbuf->y, abuf);
	
	rect= stackbuf->rect;
	for(x= cbuf->x*cbuf->y - 1; x>=0; x--)
		if(abuf[x]>1)
			rect[x]= (1.0f/255.0f)*(float)abuf[x];
	
	MEM_freeN(abuf);
}

static void node_composit_exec_idmask(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf;
		
		if(cbuf->type!=CB_VAL)
			return;
		
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs;
		
		do_idmask(stackbuf, cbuf, (float)node->custom1);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_idmask= {
	/* type code   */	CMP_NODE_ID_MASK,
	/* name        */	"ID Mask",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_idmask_in,
	/* output sock */	cmp_node_idmask_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_idmask
};


/* **************** Displace  ******************** */

static bNodeSocketType cmp_node_displace_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Vector",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "X Scale",				0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_VALUE, 1, "Y Scale",				0.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_displace_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_displace(CompBuf *stackbuf, CompBuf *cbuf, CompBuf *vecbuf, float *veccol, float *xscale, float *yscale)
{
	ImBuf *ibuf;
	int x, y, sx, sy;
	float dx=0.0, dy=0.0;
	float dspx, dspy;
	float uv[2];

	float *out= stackbuf->rect, *vec=vecbuf->rect, *in= cbuf->rect;
	float *vp, *vpnext, *vpprev;
	
	int row = 3*vecbuf->x;
	
	/* ibuf needed for sampling */
	ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0, 0);
	ibuf->rect_float= cbuf->rect;
	
	vec = vecbuf->rect;
	
	sx= stackbuf->x;
	sy= stackbuf->y;
	
	for(y=0; y<sy; y++) {
		for(x= 0; x< sx; x++, out+=4, in+=4, vec+=3) {
			
			/* the x-xrad stuff is a bit weird, but i seem to need it otherwise 
			 * my returned pixels are offset weirdly */
			vp = compbuf_get_pixel(vecbuf, veccol, x-vecbuf->xrad, y-vecbuf->yrad, vecbuf->xrad, vecbuf->yrad);
			
			/* find the new displaced co-ords, also correcting for translate offset */
			dspx = x - (*xscale * vp[0]);
			dspy = y - (*yscale * vp[1]);

			/* convert image space to 0.0-1.0 UV space for sampling, correcting for translate offset */
			uv[0] = dspx / (float)sx;
			uv[1] = dspy / (float)sy;
			
			if(x>0 && x< vecbuf->x-1 && y>0 && y< vecbuf->y-1)  {
				vpnext = vp+row;
				vpprev = vp-row;
			
				/* adaptive sampling, X channel */
				dx= 0.5f*(fabs(vp[0]-vp[-3]) + fabs(vp[0]-vp[3]));
				
				dx+= 0.25f*(fabs(vp[0]-vpprev[-3]) + fabs(vp[0]-vpnext[-3]));
				dx+= 0.25f*(fabs(vp[0]-vpprev[+3]) + fabs(vp[0]-vpnext[+3]));
				
				/* adaptive sampling, Y channel */
				dy= 0.5f*(fabs(vp[1]-vp[-row+1]) + fabs(vp[1]-vp[row+1]));
						 
				dy+= 0.25f*(fabs(vp[1]-vpprev[+1-3]) + fabs(vp[1]-vpnext[+1-3]));
				dy+= 0.25f*(fabs(vp[1]-vpprev[+1+3]) + fabs(vp[1]-vpnext[+1+3]));
				
				/* scaled down to prevent blurriness */
				/* 8: magic number, provides a good level of sharpness without getting too aliased */
				dx /= 8;
				dy /= 8;
			}

			/* should use mipmap */
			if(dx > 0.006f) dx= 0.006f;
			if(dy > 0.006f) dy= 0.006f;
			if ((vp[0]> 0.0) && (dx < 0.004)) dx = 0.004;
			if ((vp[1]> 0.0) && (dy < 0.004)) dy = 0.004;
			

			ibuf_sample(ibuf, uv[0], uv[1], dx, dy, out);
		}
	}

	IMB_freeImBuf(ibuf);	
}


static void node_composit_exec_displace(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data && in[1]->data) {
		CompBuf *cbuf= in[0]->data;
		CompBuf *vecbuf= in[1]->data;
		CompBuf *stackbuf;
		
		cbuf= typecheck_compbuf(cbuf, CB_RGBA);
		vecbuf= typecheck_compbuf(vecbuf, CB_VEC3);
		stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs

		do_displace(stackbuf, cbuf, vecbuf, in[1]->vec, in[2]->vec, in[3]->vec);
		
		out[0]->data= stackbuf;
		
		
		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
		if(vecbuf!=in[1]->data)
			free_compbuf(vecbuf);
	}
}

static bNodeType cmp_node_displace= {
	/* type code   */	CMP_NODE_DISPLACE,
	/* name        */	"Displace",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS,
	/* input sock  */	cmp_node_displace_in,
	/* output sock */	cmp_node_displace_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_displace
};

/* **************** SCALAR MATH ******************** */ 
static bNodeSocketType cmp_node_math_in[]= { 
	{ SOCK_VALUE, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f}, 
	{ SOCK_VALUE, 1, "Value", 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketType cmp_node_math_out[]= { 
	{ SOCK_VALUE, 0, "Value", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void do_math(bNode *node, float *out, float *in, float *in2)
{
	switch(node->custom1)
	{
	case 0: /* Add */
		out[0]= in[0] + in2[0]; 
		break; 
	case 1: /* Subtract */
		out[0]= in[0] - in2[0];
		break; 
	case 2: /* Multiply */
		out[0]= in[0] * in2[0]; 
		break; 
	case 3: /* Divide */
		{
			if(in[1]==0)	/* We don't want to divide by zero. */
				out[0]= 0.0;
			else
				out[0]= in[0] / in2[0];
			}
		break;
	case 4: /* Sine */
		out[0]= sin(in[0]);
		break;
	case 5: /* Cosine */
		out[0]= cos(in[0]);
		break;
	case 6: /* Tangent */
		out[0]= tan(in[0]);
		break;
	case 7: /* Arc-Sine */
		{
			/* Can't do the impossible... */
			if(in[0] <= 1 && in[0] >= -1 )
				out[0]= asin(in[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 8: /* Arc-Cosine */
		{
			/* Can't do the impossible... */
			if( in[0] <= 1 && in[0] >= -1 )
				out[0]= acos(in[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 9: /* Arc-Tangent */
		out[0]= atan(in[0]);
		break;
	case 10: /* Power */
		{
			/* Don't want any imaginary numbers... */
			if( in[0] >= 0 )
				out[0]= pow(in[0], in2[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 11: /* Logarithm */
		{
			/* Don't want any imaginary numbers... */
			if( in[0] > 0  && in2[0] > 0 )
				out[0]= log(in[0]) / log(in2[0]);
			else
				out[0]= 0.0;
		}
		break;
	case 12: /* Minimum */
		{
			if( in[0] < in2[0] )
				out[0]= in2[0];
			else
				out[0]= in[0];
		}
		break;
	case 13: /* Maximum */
		{
			if( in[0] > in2[0] )
				out[0]= in2[0];
			else
				out[0]= in[0];
		}
		break;
	case 14: /* Round */
		{
				out[0]= (int)(in[0] + 0.5f);
		}
		break; 
	}
}

static void node_composit_exec_math(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order out: bw */
	/* stack order in: col */
	
	if(out[0]->hasoutput==0)
		return;
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		do_math(node, out[0]->vec, in[0]->vec, in[1]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs
		
		composit2_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_math, CB_VAL, CB_VAL);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_math= {
	/* type code   */	CMP_NODE_MATH,
	/* name        */	"Math",
	/* width+range */	120, 110, 160,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_math_in,
	/* output sock */	cmp_node_math_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_math
};

/* ****************** types array for all shaders ****************** */

bNodeType *node_all_composit[]= {
	&node_group_typeinfo,
	
	&cmp_node_rlayers,
	&cmp_node_image,
	&cmp_node_texture,
	&cmp_node_value,
	&cmp_node_rgb,
	&cmp_node_time,
	
	&cmp_node_composite,
	&cmp_node_viewer,
	&cmp_node_splitviewer,
	&cmp_node_output_file,
	
	&cmp_node_curve_rgb,
	&cmp_node_mix_rgb,
	&cmp_node_hue_sat,
	&cmp_node_alphaover,
	&cmp_node_zcombine,
	
	&cmp_node_normal,
	&cmp_node_curve_vec,
	&cmp_node_map_value,

	&cmp_node_filter,
	&cmp_node_blur,
	&cmp_node_vecblur,
	&cmp_node_dilateerode,
	&cmp_node_defocus,

	&cmp_node_valtorgb,
	&cmp_node_rgbtobw,	
	&cmp_node_setalpha,
	&cmp_node_idmask,
	&cmp_node_math,
	&cmp_node_seprgba,
	&cmp_node_combrgba,
	&cmp_node_sephsva,
	&cmp_node_combhsva,
	&cmp_node_sepyuva,
	&cmp_node_combyuva,
	&cmp_node_sepycca,
	&cmp_node_combycca,
		
	&cmp_node_diff_matte,
	&cmp_node_chroma,
	&cmp_node_channel_matte,
	&cmp_node_color_spill,
	
	&cmp_node_translate,
	&cmp_node_rotate,
	&cmp_node_scale,
	&cmp_node_flip,
	&cmp_node_displace,
	&cmp_node_mapuv,

	NULL
};

/* ******************* parse ************ */

/* clumsy checking... should do dynamic outputs once */
static void force_hidden_passes(bNode *node, int passflag)
{
	bNodeSocket *sock;
	
	for(sock= node->outputs.first; sock; sock= sock->next)
		sock->flag &= ~SOCK_UNAVAIL;
	
	sock= BLI_findlink(&node->outputs, RRES_OUT_Z);
	if(!(passflag & SCE_PASS_Z)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_NORMAL);
	if(!(passflag & SCE_PASS_NORMAL)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_VEC);
	if(!(passflag & SCE_PASS_VECTOR)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_UV);
	if(!(passflag & SCE_PASS_UV)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_RGBA);
	if(!(passflag & SCE_PASS_RGBA)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_DIFF);
	if(!(passflag & SCE_PASS_DIFFUSE)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_SPEC);
	if(!(passflag & SCE_PASS_SPEC)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_SHADOW);
	if(!(passflag & SCE_PASS_SHADOW)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_AO);
	if(!(passflag & SCE_PASS_AO)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_REFLECT);
	if(!(passflag & SCE_PASS_REFLECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_REFRACT);
	if(!(passflag & SCE_PASS_REFRACT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_RADIO);
	if(!(passflag & SCE_PASS_RADIO)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDEXOB);
	if(!(passflag & SCE_PASS_INDEXOB)) sock->flag |= SOCK_UNAVAIL;
				
}

/* based on rules, force sockets hidden always */
void ntreeCompositForceHidden(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if( node->type==CMP_NODE_R_LAYERS) {
			Scene *sce= node->id?(Scene *)node->id:G.scene; /* G.scene is WEAK! */
			SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
			if(srl)
				force_hidden_passes(node, srl->passflag);
		}
		else if( node->type==CMP_NODE_IMAGE) {
			Image *ima= (Image *)node->id;
			if(ima) {
				if(ima->rr) {
					ImageUser *iuser= node->storage;
					RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
					if(rl)
						force_hidden_passes(node, rl->passflag);
					else
						force_hidden_passes(node, 0);
				}
				else if(ima->type!=IMA_TYPE_MULTILAYER) {	/* if ->rr not yet read we keep inputs */
					force_hidden_passes(node, RRES_OUT_Z);
				}
			}
			else
				force_hidden_passes(node, 0);
		}
	}
	
}

/* called from render pipeline, to tag render input and output */
void ntreeCompositTagRender(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if( ELEM(node->type, CMP_NODE_R_LAYERS, CMP_NODE_COMPOSITE))
			NodeTagChanged(ntree, node);
	}
}

/* tags nodes that have animation capabilities */
void ntreeCompositTagAnimated(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_IMAGE) {
			Image *ima= (Image *)node->id;
			if(ima && ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
				NodeTagChanged(ntree, node);
		}
		else if(node->type==CMP_NODE_TIME)
			NodeTagChanged(ntree, node);
	}
}


/* called from image window preview */
void ntreeCompositTagGenerators(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if( ELEM(node->type, CMP_NODE_R_LAYERS, CMP_NODE_IMAGE))
			NodeTagChanged(ntree, node);
	}
}
