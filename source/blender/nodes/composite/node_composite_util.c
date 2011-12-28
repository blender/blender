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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/node_composite_util.c
 *  \ingroup nodes
 */


#include "node_composite_util.h"

CompBuf *alloc_compbuf(int sizex, int sizey, int type, int alloc)
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

CompBuf *dupalloc_compbuf(CompBuf *cbuf)
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
CompBuf *pass_on_compbuf(CompBuf *cbuf)
{
	CompBuf *dupbuf= (cbuf)? alloc_compbuf(cbuf->x, cbuf->y, cbuf->type, 0): NULL;
	CompBuf *lastbuf;
	
	if(dupbuf) {
		dupbuf->rect= cbuf->rect;
		dupbuf->xof= cbuf->xof;
		dupbuf->yof= cbuf->yof;
		dupbuf->malloc= 0;
		
		/* get last buffer in list, and append dupbuf */
		for(lastbuf= cbuf; lastbuf; lastbuf= lastbuf->next)
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
	printf("Compbuf %s %d %d %p\n", str, cbuf->x, cbuf->y, (void *)cbuf->rect);
	
}

void compbuf_set_node(CompBuf *cbuf, bNode *node)
{
	if (cbuf) cbuf->node = node;
}

/* used for disabling node  (similar code in node_draw.c for disable line and node_edit for untangling nodes) */
void node_compo_pass_on(void *UNUSED(data), int UNUSED(thread), struct bNode *node, void *UNUSED(nodedata),
                        struct bNodeStack **in, struct bNodeStack **out)
{
	ListBase links;
	LinkInOutsMuteNode *lnk;
	int i;

	if(node->typeinfo->mutelinksfunc == NULL)
		return;

	/* Get default muting links (as bNodeStack pointers). */
	links = node->typeinfo->mutelinksfunc(NULL, node, in, out, NULL, NULL);

	for(lnk = links.first; lnk; lnk = lnk->next) {
		for(i = 0; i < lnk->num_outs; i++) {
			if(((bNodeStack*)(lnk->in))->data)
				(((bNodeStack*)(lnk->outs))+i)->data = pass_on_compbuf((CompBuf*)((bNodeStack*)(lnk->in))->data);
		}
		/* If num_outs > 1, lnk->outs was an allocated table of pointers... */
		if(i > 1)
			MEM_freeN(lnk->outs);
	}
	BLI_freelistN(&links);
}


CompBuf *get_cropped_compbuf(rcti *drect, float *rectf, int rectx, int recty, int type)
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

CompBuf *scalefast_compbuf(CompBuf *inbuf, int newx, int newy)
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

void typecheck_compbuf_color(float *out, float *in, int outtype, int intype)
{
	if(intype == outtype) {
		memcpy(out, in, sizeof(float)*outtype);
	}
	else if(outtype==CB_VAL) {
		if(intype==CB_VEC2) {
			*out= 0.5f*(in[0]+in[1]);
		}
		else if(intype==CB_VEC3) {
			*out= 0.333333f*(in[0]+in[1]+in[2]);
		}
		else if(intype==CB_RGBA) {
			*out= in[0]*0.35f + in[1]*0.45f + in[2]*0.2f;
		}
	}
	else if(outtype==CB_VEC2) {
		if(intype==CB_VAL) {
			out[0]= in[0];
			out[1]= in[0];
		}
		else if(intype==CB_VEC3) {
			out[0]= in[0];
			out[1]= in[1];
		}
		else if(intype==CB_RGBA) {
			out[0]= in[0];
			out[1]= in[1];
		}
	}
	else if(outtype==CB_VEC3) {
		if(intype==CB_VAL) {
			out[0]= in[0];
			out[1]= in[0];
			out[2]= in[0];
		}
		else if(intype==CB_VEC2) {
			out[0]= in[0];
			out[1]= in[1];
			out[2]= 0.0f;
		}
		else if(intype==CB_RGBA) {
			out[0]= in[0];
			out[1]= in[1];
			out[2]= in[2];
		}
	}
	else if(outtype==CB_RGBA) {
		if(intype==CB_VAL) {
			out[0]= in[0];
			out[1]= in[0];
			out[2]= in[0];
			out[3]= 1.0f;
		}
		else if(intype==CB_VEC2) {
			out[0]= in[0];
			out[1]= in[1];
			out[2]= 0.0f;
			out[3]= 1.0f;
		}
		else if(intype==CB_VEC3) {
			out[0]= in[0];
			out[1]= in[1];
			out[2]= in[2];
			out[3]= 1.0f;
		}
	}
}

CompBuf *typecheck_compbuf(CompBuf *inbuf, int type)
{
	if(inbuf && inbuf->type!=type) {
		CompBuf *outbuf;
		float *inrf, *outrf;
		int x;

		outbuf= alloc_compbuf(inbuf->x, inbuf->y, type, 1); 

		/* warning note: xof and yof are applied in pixelprocessor, but should be copied otherwise? */
		outbuf->xof= inbuf->xof;
		outbuf->yof= inbuf->yof;

		if(inbuf->rect_procedural) {
			outbuf->rect_procedural= inbuf->rect_procedural;
			copy_v3_v3(outbuf->procedural_size, inbuf->procedural_size);
			copy_v3_v3(outbuf->procedural_offset, inbuf->procedural_offset);
			outbuf->procedural_type= inbuf->procedural_type;
			outbuf->node= inbuf->node;
			return outbuf;
		}

		inrf= inbuf->rect;
		outrf= outbuf->rect;
		x= inbuf->x*inbuf->y;
		
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
					outrf[3]= 1.0f;
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

static float *compbuf_get_pixel(CompBuf *cbuf, float *defcol, float *use, int x, int y, int xrad, int yrad)
{
	if(cbuf) {
		if(cbuf->rect_procedural) {
			cbuf->rect_procedural(cbuf, use, (float)x/(float)xrad, (float)y/(float)yrad);
			return use;
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
	else return defcol;
}

/* **************************************************** */

/* Pixel-to-Pixel operation, 1 Image in, 1 out */
void composit1_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
									  void (*func)(bNode *, float *, float *), 
									  int src_type)
{
	CompBuf *src_use;
	float *outfp=out->rect, *srcfp;
	float color[4];	/* local color if compbuf is procedural */
	int xrad, yrad, x, y;
	
	src_use= typecheck_compbuf(src_buf, src_type);
	
	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			srcfp= compbuf_get_pixel(src_use, src_col, color, x, y, xrad, yrad);
			func(node, outfp, srcfp);
		}
	}
	
	if(src_use!=src_buf)
		free_compbuf(src_use);
}

/* Pixel-to-Pixel operation, 2 Images in, 1 out */
void composit2_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
									  CompBuf *fac_buf, float *fac, void (*func)(bNode *, float *, float *, float *), 
									  int src_type, int fac_type)
{
	CompBuf *src_use, *fac_use;
	float *outfp=out->rect, *srcfp, *facfp;
	float color[4];	/* local color if compbuf is procedural */
	int xrad, yrad, x, y;
	
	src_use= typecheck_compbuf(src_buf, src_type);
	fac_use= typecheck_compbuf(fac_buf, fac_type);

	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			srcfp= compbuf_get_pixel(src_use, src_col, color, x, y, xrad, yrad);
			facfp= compbuf_get_pixel(fac_use, fac, color, x, y, xrad, yrad);
			
			func(node, outfp, srcfp, facfp);
		}
	}
	if(src_use!=src_buf)
		free_compbuf(src_use);
	if(fac_use!=fac_buf)
		free_compbuf(fac_use);
}

/* Pixel-to-Pixel operation, 3 Images in, 1 out */
void composit3_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *src2_buf, float *src2_col, 
									  CompBuf *fac_buf, float *fac, void (*func)(bNode *, float *, float *, float *, float *), 
									  int src1_type, int src2_type, int fac_type)
{
	CompBuf *src1_use, *src2_use, *fac_use;
	float *outfp=out->rect, *src1fp, *src2fp, *facfp;
	float color[4];	/* local color if compbuf is procedural */
	int xrad, yrad, x, y;
	
	src1_use= typecheck_compbuf(src1_buf, src1_type);
	src2_use= typecheck_compbuf(src2_buf, src2_type);
	fac_use= typecheck_compbuf(fac_buf, fac_type);
	
	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			src1fp= compbuf_get_pixel(src1_use, src1_col, color, x, y, xrad, yrad);
			src2fp= compbuf_get_pixel(src2_use, src2_col, color, x, y, xrad, yrad);
			facfp= compbuf_get_pixel(fac_use, fac, color, x, y, xrad, yrad);
			
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
void composit4_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *fac1_buf, float *fac1, 
									  CompBuf *src2_buf, float *src2_col, CompBuf *fac2_buf, float *fac2, 
									  void (*func)(bNode *, float *, float *, float *, float *, float *), 
									  int src1_type, int fac1_type, int src2_type, int fac2_type)
{
	CompBuf *src1_use, *src2_use, *fac1_use, *fac2_use;
	float *outfp=out->rect, *src1fp, *src2fp, *fac1fp, *fac2fp;
	float color[4];	/* local color if compbuf is procedural */
	int xrad, yrad, x, y;
	
	src1_use= typecheck_compbuf(src1_buf, src1_type);
	src2_use= typecheck_compbuf(src2_buf, src2_type);
	fac1_use= typecheck_compbuf(fac1_buf, fac1_type);
	fac2_use= typecheck_compbuf(fac2_buf, fac2_type);
	
	xrad= out->xrad;
	yrad= out->yrad;
	
	for(y= -yrad; y<-yrad+out->y; y++) {
		for(x= -xrad; x<-xrad+out->x; x++, outfp+=out->type) {
			src1fp= compbuf_get_pixel(src1_use, src1_col, color, x, y, xrad, yrad);
			src2fp= compbuf_get_pixel(src2_use, src2_col, color, x, y, xrad, yrad);
			fac1fp= compbuf_get_pixel(fac1_use, fac1, color, x, y, xrad, yrad);
			fac2fp= compbuf_get_pixel(fac2_use, fac2, color, x, y, xrad, yrad);
			
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


CompBuf *valbuf_from_rgbabuf(CompBuf *cbuf, int channel)
{
	CompBuf *valbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
	float *valf, *rectf;
	int tot;
	
	/* warning note: xof and yof are applied in pixelprocessor, but should be copied otherwise? */
	valbuf->xof= cbuf->xof;
	valbuf->yof= cbuf->yof;
	
	valf= valbuf->rect;

	/* defaults to returning alpha channel */
	if ((channel < CHAN_R) || (channel > CHAN_A)) channel = CHAN_A;

	rectf= cbuf->rect + channel;
	
	for(tot= cbuf->x*cbuf->y; tot>0; tot--, valf++, rectf+=4)
		*valf= *rectf;
	
	return valbuf;
}

static CompBuf *generate_procedural_preview(CompBuf *cbuf, int newx, int newy)
{
	CompBuf *outbuf;
	float *outfp;
	int xrad, yrad, x, y;
	
	outbuf= alloc_compbuf(newx, newy, CB_RGBA, 1);

	outfp= outbuf->rect;
	xrad= outbuf->xrad;
	yrad= outbuf->yrad;
	
	for(y= -yrad; y<-yrad+outbuf->y; y++)
		for(x= -xrad; x<-xrad+outbuf->x; x++, outfp+=outbuf->type)
			cbuf->rect_procedural(cbuf, outfp, (float)x/(float)xrad, (float)y/(float)yrad);

	return outbuf;
}

void generate_preview(void *data, bNode *node, CompBuf *stackbuf)
{
	RenderData *rd= data;
	bNodePreview *preview= node->preview;
	int xsize, ysize;
	int profile_from= (rd->color_mgt_flag & R_COLOR_MANAGEMENT)? IB_PROFILE_LINEAR_RGB: IB_PROFILE_SRGB;
	int predivide= 0;
	int dither= 0;
	unsigned char *rect;
	
	if(preview && stackbuf) {
		CompBuf *cbuf, *stackbuf_use;
		
		if(stackbuf->rect==NULL && stackbuf->rect_procedural==NULL) return;
		
		stackbuf_use= typecheck_compbuf(stackbuf, CB_RGBA);

		if(stackbuf->x > stackbuf->y) {
			xsize= 140;
			ysize= (140*stackbuf->y)/stackbuf->x;
		}
		else {
			ysize= 140;
			xsize= (140*stackbuf->x)/stackbuf->y;
		}
		
		if(stackbuf_use->rect_procedural)
			cbuf= generate_procedural_preview(stackbuf_use, xsize, ysize);
		else
			cbuf= scalefast_compbuf(stackbuf_use, xsize, ysize);

		/* convert to byte for preview */
		rect= MEM_callocN(sizeof(unsigned char)*4*xsize*ysize, "bNodePreview.rect");

		IMB_buffer_byte_from_float(rect, cbuf->rect,
			4, dither, IB_PROFILE_SRGB, profile_from, predivide, 
			xsize, ysize, xsize, xsize);
		
		free_compbuf(cbuf);
		if(stackbuf_use!=stackbuf)
			free_compbuf(stackbuf_use);

		BLI_lock_thread(LOCK_PREVIEW);

		if(preview->rect)
			MEM_freeN(preview->rect);
		preview->xsize= xsize;
		preview->ysize= ysize;
		preview->rect= rect;

		BLI_unlock_thread(LOCK_PREVIEW);
	}
}

void do_rgba_to_yuva(bNode *UNUSED(node), float *out, float *in)
{
	rgb_to_yuv(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[3]=in[3];
}

void do_rgba_to_hsva(bNode *UNUSED(node), float *out, float *in)
{
	rgb_to_hsv(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[3]=in[3];
}

void do_rgba_to_ycca(bNode *UNUSED(node), float *out, float *in)
{
	rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2], BLI_YCC_ITU_BT601);
	out[3]=in[3];
}

void do_yuva_to_rgba(bNode *UNUSED(node), float *out, float *in)
{
	yuv_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[3]=in[3];
}

void do_hsva_to_rgba(bNode *UNUSED(node), float *out, float *in)
{
	hsv_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
	out[3]=in[3];
}

void do_ycca_to_rgba(bNode *UNUSED(node), float *out, float *in)
{
	ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2], BLI_YCC_ITU_BT601);
	out[3]=in[3];
}

void do_copy_rgba(bNode *UNUSED(node), float *out, float *in)
{
	copy_v4_v4(out, in);
}

void do_copy_rgb(bNode *UNUSED(node), float *out, float *in)
{
	copy_v3_v3(out, in);
	out[3]= 1.0f;
}

void do_copy_value(bNode *UNUSED(node), float *out, float *in)
{
	out[0]= in[0];
}

void do_copy_a_rgba(bNode *UNUSED(node), float *out, float *in, float *fac)
{
	copy_v3_v3(out, in);
	out[3]= *fac;
}

/* only accepts RGBA buffers */
void gamma_correct_compbuf(CompBuf *img, int inversed)
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

void premul_compbuf(CompBuf *img, int inversed)
{
	float *drect;
	int x;

	if(img->type!=CB_RGBA) return;

	drect= img->rect;
	if(inversed) {
		for(x=img->x*img->y; x>0; x--, drect+=4) {
			if(fabsf(drect[3]) < 1e-5f) {
				drect[0]= 0.0f;
				drect[1]= 0.0f;
				drect[2]= 0.0f;
			}
			else {
				drect[0] /= drect[3];
				drect[1] /= drect[3];
				drect[2] /= drect[3];
			}
		}
	}
	else {
		for(x=img->x*img->y; x>0; x--, drect+=4) {
			drect[0] *= drect[3];
			drect[1] *= drect[3];
			drect[2] *= drect[3];
		}
	}
}



/*
 *  2D Fast Hartley Transform, used for convolution
 */

typedef float fREAL;

// returns next highest power of 2 of x, as well it's log2 in L2
static unsigned int nextPow2(unsigned int x, unsigned int* L2)
{
	unsigned int pw, x_notpow2 = x & (x-1);
	*L2 = 0;
	while (x>>=1) ++(*L2);
	pw = 1 << (*L2);
	if (x_notpow2) { (*L2)++;  pw<<=1; }
	return pw;
}

//------------------------------------------------------------------------------

// from FXT library by Joerg Arndt, faster in order bitreversal
// use: r = revbin_upd(r, h) where h = N>>1
static unsigned int revbin_upd(unsigned int r, unsigned int h)
{
	while (!((r^=h)&h)) h >>= 1;
	return r;
}
//------------------------------------------------------------------------------
static void FHT(fREAL* data, unsigned int M, unsigned int inverse)
{
	double tt, fc, dc, fs, ds, a = M_PI;
	fREAL t1, t2;
	int n2, bd, bl, istep, k, len = 1 << M, n = 1;

	int i, j = 0;
	unsigned int Nh = len >> 1;
	for (i=1;i<(len-1);++i) {
		j = revbin_upd(j, Nh);
		if (j>i) {
			t1 = data[i];
			data[i] = data[j];
			data[j] = t1;
		}
	}

	do {
		fREAL* data_n = &data[n];

		istep = n << 1;
		for (k=0; k<len; k+=istep) {
			t1 = data_n[k];
			data_n[k] = data[k] - t1;
			data[k] += t1;
		}

		n2 = n >> 1;
		if (n>2) {
			fc = dc = cos(a);
			fs = ds = sqrt(1.0 - fc*fc); //sin(a);
			bd = n-2;
			for (bl=1; bl<n2; bl++) {
				fREAL* data_nbd = &data_n[bd];
				fREAL* data_bd = &data[bd];
				for (k=bl; k<len; k+=istep) {
					t1 = fc*data_n[k] + fs*data_nbd[k];
					t2 = fs*data_n[k] - fc*data_nbd[k];
					data_n[k] = data[k] - t1;
					data_nbd[k] = data_bd[k] - t2;
					data[k] += t1;
					data_bd[k] += t2;
				}
				tt = fc*dc - fs*ds;
				fs = fs*dc + fc*ds;
				fc = tt;
				bd -= 2;
			}
		}

		if (n>1) {
			for (k=n2; k<len; k+=istep) {
				t1 = data_n[k];
				data_n[k] = data[k] - t1;
				data[k] += t1;
			}
		}

		n = istep;
		a *= 0.5;
	} while (n<len);

	if (inverse) {
		fREAL sc = (fREAL)1 / (fREAL)len;
		for (k=0; k<len; ++k)
			data[k] *= sc;
	}
}
//------------------------------------------------------------------------------
/* 2D Fast Hartley Transform, Mx/My -> log2 of width/height,
	nzp -> the row where zero pad data starts,
	inverse -> see above */
static void FHT2D(fREAL *data, unsigned int Mx, unsigned int My,
		unsigned int nzp, unsigned int inverse)
{
	unsigned int i, j, Nx, Ny, maxy;
	fREAL t;

	Nx = 1 << Mx;
	Ny = 1 << My;

	// rows (forward transform skips 0 pad data)
	maxy = inverse ? Ny : nzp;
	for (j=0; j<maxy; ++j)
		FHT(&data[Nx*j], Mx, inverse);

	// transpose data
	if (Nx==Ny) {  // square
		for (j=0; j<Ny; ++j)
			for (i=j+1; i<Nx; ++i) {
				unsigned int op = i + (j << Mx), np = j + (i << My);
				t=data[op], data[op]=data[np], data[np]=t;
			}
	}
	else {  // rectangular
		unsigned int k, Nym = Ny-1, stm = 1 << (Mx + My);
		for (i=0; stm>0; i++) {
			#define pred(k) (((k & Nym) << Mx) + (k >> My))
			for (j=pred(i); j>i; j=pred(j));
			if (j < i) continue;
			for (k=i, j=pred(i); j!=i; k=j, j=pred(j), stm--)
				{ t=data[j], data[j]=data[k], data[k]=t; }
			#undef pred
			stm--;
		}
	}
	// swap Mx/My & Nx/Ny
	i = Nx, Nx = Ny, Ny = i;
	i = Mx, Mx = My, My = i;

	// now columns == transposed rows
	for (j=0; j<Ny; ++j)
		FHT(&data[Nx*j], Mx, inverse);

	// finalize
	for (j=0; j<=(Ny >> 1); j++) {
		unsigned int jm = (Ny - j) & (Ny-1);
		unsigned int ji = j << Mx;
		unsigned int jmi = jm << Mx;
		for (i=0; i<=(Nx >> 1); i++) {
			unsigned int im = (Nx - i) & (Nx-1);
			fREAL A = data[ji + i];
			fREAL B = data[jmi + i];
			fREAL C = data[ji + im];
			fREAL D = data[jmi + im];
			fREAL E = (fREAL)0.5*((A + D) - (B + C));
			data[ji + i] = A - E;
			data[jmi + i] = B + E;
			data[ji + im] = C + E;
			data[jmi + im] = D - E;
		}
	}

}

//------------------------------------------------------------------------------

/* 2D convolution calc, d1 *= d2, M/N - > log2 of width/height */
static void fht_convolve(fREAL* d1, fREAL* d2, unsigned int M, unsigned int N)
{
	fREAL a, b;
	unsigned int i, j, k, L, mj, mL;
	unsigned int m = 1 << M, n = 1 << N;
	unsigned int m2 = 1 << (M-1), n2 = 1 << (N-1);
	unsigned int mn2 = m << (N-1);

	d1[0] *= d2[0];
	d1[mn2] *= d2[mn2];
	d1[m2] *= d2[m2];
	d1[m2 + mn2] *= d2[m2 + mn2];
	for (i=1; i<m2; i++) {
		k = m - i;
		a = d1[i]*d2[i] - d1[k]*d2[k];
		b = d1[k]*d2[i] + d1[i]*d2[k];
		d1[i] = (b + a)*(fREAL)0.5;
		d1[k] = (b - a)*(fREAL)0.5;
		a = d1[i + mn2]*d2[i + mn2] - d1[k + mn2]*d2[k + mn2];
		b = d1[k + mn2]*d2[i + mn2] + d1[i + mn2]*d2[k + mn2];
		d1[i + mn2] = (b + a)*(fREAL)0.5;
		d1[k + mn2] = (b - a)*(fREAL)0.5;
	}
	for (j=1; j<n2; j++) {
		L = n - j;
		mj = j << M;
		mL = L << M;
		a = d1[mj]*d2[mj] - d1[mL]*d2[mL];
		b = d1[mL]*d2[mj] + d1[mj]*d2[mL];
		d1[mj] = (b + a)*(fREAL)0.5;
		d1[mL] = (b - a)*(fREAL)0.5;
		a = d1[m2 + mj]*d2[m2 + mj] - d1[m2 + mL]*d2[m2 + mL];
		b = d1[m2 + mL]*d2[m2 + mj] + d1[m2 + mj]*d2[m2 + mL];
		d1[m2 + mj] = (b + a)*(fREAL)0.5;
		d1[m2 + mL] = (b - a)*(fREAL)0.5;
	}
	for (i=1; i<m2; i++) {
		k = m - i;
		for (j=1; j<n2; j++) {
			L = n - j;
			mj = j << M;
			mL = L << M;
			a = d1[i + mj]*d2[i + mj] - d1[k + mL]*d2[k + mL];
			b = d1[k + mL]*d2[i + mj] + d1[i + mj]*d2[k + mL];
			d1[i + mj] = (b + a)*(fREAL)0.5;
			d1[k + mL] = (b - a)*(fREAL)0.5;
			a = d1[i + mL]*d2[i + mL] - d1[k + mj]*d2[k + mj];
			b = d1[k + mj]*d2[i + mL] + d1[i + mL]*d2[k + mj];
			d1[i + mL] = (b + a)*(fREAL)0.5;
			d1[k + mj] = (b - a)*(fREAL)0.5;
		}
	}
}

//------------------------------------------------------------------------------

void convolve(CompBuf* dst, CompBuf* in1, CompBuf* in2)
{
	fREAL *data1, *data2, *fp;
	unsigned int w2, h2, hw, hh, log2_w, log2_h;
	fRGB wt, *colp;
	int x, y, ch;
	int xbl, ybl, nxb, nyb, xbsz, ybsz;
	int in2done = 0;

	CompBuf* rdst = alloc_compbuf(in1->x, in1->y, in1->type, 1);

	// convolution result width & height
	w2 = 2*in2->x - 1;
	h2 = 2*in2->y - 1;
	// FFT pow2 required size & log2
	w2 = nextPow2(w2, &log2_w);
	h2 = nextPow2(h2, &log2_h);

	// alloc space
	data1 = (fREAL*)MEM_callocN(3*w2*h2*sizeof(fREAL), "convolve_fast FHT data1");
	data2 = (fREAL*)MEM_callocN(w2*h2*sizeof(fREAL), "convolve_fast FHT data2");

	// normalize convolutor
	wt[0] = wt[1] = wt[2] = 0.f;
	for (y=0; y<in2->y; y++) {
		colp = (fRGB*)&in2->rect[y*in2->x*in2->type];
		for (x=0; x<in2->x; x++)
			fRGB_add(wt, colp[x]);
	}
	if (wt[0] != 0.f) wt[0] = 1.f/wt[0];
	if (wt[1] != 0.f) wt[1] = 1.f/wt[1];
	if (wt[2] != 0.f) wt[2] = 1.f/wt[2];
	for (y=0; y<in2->y; y++) {
		colp = (fRGB*)&in2->rect[y*in2->x*in2->type];
		for (x=0; x<in2->x; x++)
			fRGB_colormult(colp[x], wt);
	}

	// copy image data, unpacking interleaved RGBA into separate channels
	// only need to calc data1 once

	// block add-overlap
	hw = in2->x >> 1;
	hh = in2->y >> 1;
	xbsz = (w2 + 1) - in2->x;
	ybsz = (h2 + 1) - in2->y;
	nxb = in1->x / xbsz;
	if (in1->x % xbsz) nxb++;
	nyb = in1->y / ybsz;
	if (in1->y % ybsz) nyb++;
	for (ybl=0; ybl<nyb; ybl++) {
		for (xbl=0; xbl<nxb; xbl++) {

			// each channel one by one
			for (ch=0; ch<3; ch++) {
				fREAL* data1ch = &data1[ch*w2*h2];

				// only need to calc fht data from in2 once, can re-use for every block
				if (!in2done) {
					// in2, channel ch -> data1
					for (y=0; y<in2->y; y++) {
						fp = &data1ch[y*w2];
						colp = (fRGB*)&in2->rect[y*in2->x*in2->type];
						for (x=0; x<in2->x; x++)
							fp[x] = colp[x][ch];
					}
				}

				// in1, channel ch -> data2
				memset(data2, 0, w2*h2*sizeof(fREAL));
				for (y=0; y<ybsz; y++) {
					int yy = ybl*ybsz + y;
					if (yy >= in1->y) continue;
					fp = &data2[y*w2];
					colp = (fRGB*)&in1->rect[yy*in1->x*in1->type];
					for (x=0; x<xbsz; x++) {
						int xx = xbl*xbsz + x;
						if (xx >= in1->x) continue;
						fp[x] = colp[xx][ch];
					}
				}

				// forward FHT
				// zero pad data start is different for each == height+1
				if (!in2done) FHT2D(data1ch, log2_w, log2_h, in2->y+1, 0);
				FHT2D(data2, log2_w, log2_h, in2->y+1, 0);

				// FHT2D transposed data, row/col now swapped
				// convolve & inverse FHT
				fht_convolve(data2, data1ch, log2_h, log2_w);
				FHT2D(data2, log2_h, log2_w, 0, 1);
				// data again transposed, so in order again

				// overlap-add result
				for (y=0; y<(int)h2; y++) {
					const int yy = ybl*ybsz + y - hh;
					if ((yy < 0) || (yy >= in1->y)) continue;
					fp = &data2[y*w2];
					colp = (fRGB*)&rdst->rect[yy*in1->x*in1->type];
					for (x=0; x<(int)w2; x++) {
						const int xx = xbl*xbsz + x - hw;
						if ((xx < 0) || (xx >= in1->x)) continue;
						colp[xx][ch] += fp[x];
					}
				}

			}
			in2done = 1;
		}
	}

	MEM_freeN(data2);
	MEM_freeN(data1);
	memcpy(dst->rect, rdst->rect, sizeof(float)*dst->x*dst->y*dst->type);
	free_compbuf(rdst);
}


/*
 *
 * Utility functions qd_* should probably be intergrated better with other functions here.
 *
 */
// sets fcol to pixelcolor at (x, y)
void qd_getPixel(CompBuf* src, int x, int y, float* col)
{
	if(src->rect_procedural) {
		float bc[4];
		src->rect_procedural(src, bc, (float)x/(float)src->xrad, (float)y/(float)src->yrad);

		switch(src->type){
			/* these fallthrough to get all the channels */
			case CB_RGBA: col[3]=bc[3]; 
			case CB_VEC3: col[2]=bc[2];
			case CB_VEC2: col[1]=bc[1];
			case CB_VAL: col[0]=bc[0];
		}
	}
	else if ((x >= 0) && (x < src->x) && (y >= 0) && (y < src->y)) {
		float* bc = &src->rect[(x + y*src->x)*src->type];
		switch(src->type){
			/* these fallthrough to get all the channels */
			case CB_RGBA: col[3]=bc[3]; 
			case CB_VEC3: col[2]=bc[2];
			case CB_VEC2: col[1]=bc[1];
			case CB_VAL: col[0]=bc[0];
		}
	}
	else {
		switch(src->type){
			/* these fallthrough to get all the channels */
			case CB_RGBA: col[3]=0.0; 
			case CB_VEC3: col[2]=0.0; 
			case CB_VEC2: col[1]=0.0; 
			case CB_VAL: col[0]=0.0; 
		}
	}
}

// sets pixel (x, y) to color col
void qd_setPixel(CompBuf* src, int x, int y, float* col)
{
	if ((x >= 0) && (x < src->x) && (y >= 0) && (y < src->y)) {
		float* bc = &src->rect[(x + y*src->x)*src->type];
		switch(src->type){
			/* these fallthrough to get all the channels */
			case CB_RGBA: bc[3]=col[3]; 
			case CB_VEC3: bc[2]=col[2];
			case CB_VEC2: bc[1]=col[1];
			case CB_VAL: bc[0]=col[0];
		}
	}
}

// adds fcol to pixelcolor (x, y)
void qd_addPixel(CompBuf* src, int x, int y, float* col)
{
	if ((x >= 0) && (x < src->x) && (y >= 0) && (y < src->y)) {
		float* bc = &src->rect[(x + y*src->x)*src->type];
		bc[0] += col[0], bc[1] += col[1], bc[2] += col[2];
	}
}

// multiplies pixel by factor value f
void qd_multPixel(CompBuf* src, int x, int y, float f)
{
	if ((x >= 0) && (x < src->x) && (y >= 0) && (y < src->y)) {
		float* bc = &src->rect[(x + y*src->x)*src->type];
		bc[0] *= f, bc[1] *= f, bc[2] *= f;
	}
}

// bilinear interpolation with wraparound
void qd_getPixelLerpWrap(CompBuf* src, float u, float v, float* col)
{
	const float ufl = floor(u), vfl = floor(v);
	const int nx = (int)ufl % src->x, ny = (int)vfl % src->y;
	const int x1 = (nx < 0) ? (nx + src->x) : nx;
	const int y1 = (ny < 0) ? (ny + src->y) : ny;
	const int x2 = (x1 + 1) % src->x, y2 = (y1 + 1) % src->y;
	const float* c00 = &src->rect[(x1 + y1*src->x)*src->type];
	const float* c10 = &src->rect[(x2 + y1*src->x)*src->type];
	const float* c01 = &src->rect[(x1 + y2*src->x)*src->type];
	const float* c11 = &src->rect[(x2 + y2*src->x)*src->type];
	const float uf = u - ufl, vf = v - vfl;
	const float w00=(1.f-uf)*(1.f-vf), w10=uf*(1.f-vf), w01=(1.f-uf)*vf, w11=uf*vf;
	col[0] = w00*c00[0] + w10*c10[0] + w01*c01[0] + w11*c11[0];
	if (src->type != CB_VAL) {
		col[1] = w00*c00[1] + w10*c10[1] + w01*c01[1] + w11*c11[1];
		col[2] = w00*c00[2] + w10*c10[2] + w01*c01[2] + w11*c11[2];
		col[3] = w00*c00[3] + w10*c10[3] + w01*c01[3] + w11*c11[3];
	}
}

// as above, without wrap around
void qd_getPixelLerp(CompBuf* src, float u, float v, float* col)
{
	const float ufl = floor(u), vfl = floor(v);
	const int x1 = (int)ufl, y1 = (int)vfl;
	const int x2 = (int)ceil(u), y2 = (int)ceil(v);
	if ((x2 >= 0) && (y2 >= 0) && (x1 < src->x) && (y1 < src->y)) {
		const float B[4] = {0,0,0,0};
		const int ox1 = (x1 < 0), oy1 = (y1 < 0), ox2 = (x2 >= src->x), oy2 = (y2 >= src->y);
		const float* c00 = (ox1 || oy1) ? B : &src->rect[(x1 + y1*src->x)*src->type];
		const float* c10 = (ox2 || oy1) ? B : &src->rect[(x2 + y1*src->x)*src->type];
		const float* c01 = (ox1 || oy2) ? B : &src->rect[(x1 + y2*src->x)*src->type];
		const float* c11 = (ox2 || oy2) ? B : &src->rect[(x2 + y2*src->x)*src->type];
		const float uf = u - ufl, vf = v - vfl;
		const float w00=(1.f-uf)*(1.f-vf), w10=uf*(1.f-vf), w01=(1.f-uf)*vf, w11=uf*vf;
		col[0] = w00*c00[0] + w10*c10[0] + w01*c01[0] + w11*c11[0];
		if (src->type != CB_VAL) {
			col[1] = w00*c00[1] + w10*c10[1] + w01*c01[1] + w11*c11[1];
			col[2] = w00*c00[2] + w10*c10[2] + w01*c01[2] + w11*c11[2];
			col[3] = w00*c00[3] + w10*c10[3] + w01*c01[3] + w11*c11[3];
		}
	}
	else col[0] = col[1] = col[2] = col[3] = 0.f;
}

// as above, sampling only one channel
void qd_getPixelLerpChan(CompBuf* src, float u, float v, int chan, float* out)
{
	const float ufl = floor(u), vfl = floor(v);
	const int x1 = (int)ufl, y1 = (int)vfl;
	const int x2 = (int)ceil(u), y2 = (int)ceil(v);
	if (chan >= src->type) chan = 0;
	if ((x2 >= 0) && (y2 >= 0) && (x1 < src->x) && (y1 < src->y)) {
		const float B[4] = {0,0,0,0};
		const int ox1 = (x1 < 0), oy1 = (y1 < 0), ox2 = (x2 >= src->x), oy2 = (y2 >= src->y);
		const float* c00 = (ox1 || oy1) ? B : &src->rect[(x1 + y1*src->x)*src->type + chan];
		const float* c10 = (ox2 || oy1) ? B : &src->rect[(x2 + y1*src->x)*src->type + chan];
		const float* c01 = (ox1 || oy2) ? B : &src->rect[(x1 + y2*src->x)*src->type + chan];
		const float* c11 = (ox2 || oy2) ? B : &src->rect[(x2 + y2*src->x)*src->type + chan];
		const float uf = u - ufl, vf = v - vfl;
		const float w00=(1.f-uf)*(1.f-vf), w10=uf*(1.f-vf), w01=(1.f-uf)*vf, w11=uf*vf;
		out[0] = w00*c00[0] + w10*c10[0] + w01*c01[0] + w11*c11[0];
	}
	else *out = 0.f;
}


CompBuf* qd_downScaledCopy(CompBuf* src, int scale)
{
	CompBuf* fbuf;
	if (scale <= 1)
		fbuf = dupalloc_compbuf(src);
	else {
		int nw = src->x/scale, nh = src->y/scale;
		if ((2*(src->x % scale)) > scale) nw++;
		if ((2*(src->y % scale)) > scale) nh++;
		fbuf = alloc_compbuf(nw, nh, src->type, 1);
		{
			int x, y, xx, yy, sx, sy, mx, my;
			float colsum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
			float fscale = 1.f/(float)(scale*scale);
			for (y=0; y<nh; y++) {
				fRGB* fcolp = (fRGB*)&fbuf->rect[y*fbuf->x*fbuf->type];
				yy = y*scale;
				my = yy + scale;
				if (my > src->y) my = src->y;
				for (x=0; x<nw; x++) {
					xx = x*scale;
					mx = xx + scale;
					if (mx > src->x) mx = src->x;
					colsum[0] = colsum[1] = colsum[2] = 0.f;
					for (sy=yy; sy<my; sy++) {
						fRGB* scolp = (fRGB*)&src->rect[sy*src->x*src->type];
						for (sx=xx; sx<mx; sx++)
							fRGB_add(colsum, scolp[sx]);
					}
					fRGB_mult(colsum, fscale);
					fRGB_copy(fcolp[x], colsum);
				}
			}
		}
	}
	return fbuf;
}

// fast g.blur, per channel
// xy var. bits 1 & 2 ca be used to blur in x or y direction separately
void IIR_gauss(CompBuf* src, float sigma, int chan, int xy)
{
	double q, q2, sc, cf[4], tsM[9], tsu[3], tsv[3];
	double *X, *Y, *W;
	int i, x, y, sz;

	// <0.5 not valid, though can have a possibly useful sort of sharpening effect
	if (sigma < 0.5f) return;
	
	if ((xy < 1) || (xy > 3)) xy = 3;
	
	// XXX The YVV macro defined below explicitly expects sources of at least 3x3 pixels,
	//     so just skiping blur along faulty direction if src's def is below that limit!
	if (src->x < 3) xy &= ~(int) 1;
	if (src->y < 3) xy &= ~(int) 2;
	if (xy < 1) return;

	// see "Recursive Gabor Filtering" by Young/VanVliet
	// all factors here in double.prec. Required, because for single.prec it seems to blow up if sigma > ~200
	if (sigma >= 3.556f)
		q = 0.9804f*(sigma - 3.556f) + 2.5091f;
	else // sigma >= 0.5
		q = (0.0561f*sigma + 0.5784f)*sigma - 0.2568f;
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
	// but neither seem to be quite the same, result seems to be ok so far anyway.
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

#define YVV(L)                                                                \
{                                                                             \
	W[0] = cf[0]*X[0] + cf[1]*X[0] + cf[2]*X[0] + cf[3]*X[0];                 \
	W[1] = cf[0]*X[1] + cf[1]*W[0] + cf[2]*X[0] + cf[3]*X[0];                 \
	W[2] = cf[0]*X[2] + cf[1]*W[1] + cf[2]*W[0] + cf[3]*X[0];                 \
	for (i=3; i<L; i++)                                                       \
		W[i] = cf[0]*X[i] + cf[1]*W[i-1] + cf[2]*W[i-2] + cf[3]*W[i-3];       \
	tsu[0] = W[L-1] - X[L-1];                                                 \
	tsu[1] = W[L-2] - X[L-1];                                                 \
	tsu[2] = W[L-3] - X[L-1];                                                 \
	tsv[0] = tsM[0]*tsu[0] + tsM[1]*tsu[1] + tsM[2]*tsu[2] + X[L-1];          \
	tsv[1] = tsM[3]*tsu[0] + tsM[4]*tsu[1] + tsM[5]*tsu[2] + X[L-1];          \
	tsv[2] = tsM[6]*tsu[0] + tsM[7]*tsu[1] + tsM[8]*tsu[2] + X[L-1];          \
	Y[L-1] = cf[0]*W[L-1] + cf[1]*tsv[0] + cf[2]*tsv[1] + cf[3]*tsv[2];       \
	Y[L-2] = cf[0]*W[L-2] + cf[1]*Y[L-1] + cf[2]*tsv[0] + cf[3]*tsv[1];       \
	Y[L-3] = cf[0]*W[L-3] + cf[1]*Y[L-2] + cf[2]*Y[L-1] + cf[3]*tsv[0];       \
	for (i=L-4; i>=0; i--)                                                    \
		Y[i] = cf[0]*W[i] + cf[1]*Y[i+1] + cf[2]*Y[i+2] + cf[3]*Y[i+3];       \
}

	// intermediate buffers
	sz = MAX2(src->x, src->y);
	X = MEM_callocN(sz*sizeof(double), "IIR_gauss X buf");
	Y = MEM_callocN(sz*sizeof(double), "IIR_gauss Y buf");
	W = MEM_callocN(sz*sizeof(double), "IIR_gauss W buf");
	if (xy & 1) {	// H
		for (y=0; y<src->y; ++y) {
			const int yx = y*src->x;
			for (x=0; x<src->x; ++x)
				X[x] = src->rect[(x + yx)*src->type + chan];
			YVV(src->x);
			for (x=0; x<src->x; ++x)
				src->rect[(x + yx)*src->type + chan] = Y[x];
		}
	}
	if (xy & 2) {	// V
		for (x=0; x<src->x; ++x) {
			for (y=0; y<src->y; ++y)
				X[y] = src->rect[(x + y*src->x)*src->type + chan];
			YVV(src->y);
			for (y=0; y<src->y; ++y)
				src->rect[(x + y*src->x)*src->type + chan] = Y[y];
		}
	}

	MEM_freeN(X);
	MEM_freeN(W);
	MEM_freeN(Y);
#undef YVV
}

