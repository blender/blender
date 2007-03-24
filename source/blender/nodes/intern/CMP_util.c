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

#include "CMP_util.h"




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

CompBuf *typecheck_compbuf(CompBuf *inbuf, int type)
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
void composit1_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
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
void composit2_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
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
void composit3_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *src2_buf, float *src2_col, 
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
void composit4_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *fac1_buf, float *fac1, 
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
	if ((channel < CHAN_R) && (channel > CHAN_A)) channel = CHAN_A;

	rectf= cbuf->rect + channel;
	
	for(tot= cbuf->x*cbuf->y; tot>0; tot--, valf++, rectf+=4)
		*valf= *rectf;
	
	return valbuf;
}

void generate_preview(bNode *node, CompBuf *stackbuf)
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

void do_rgba_to_yuva(bNode *node, float *out, float *in)
{
   rgb_to_yuv(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
   out[3]=in[3];
}

void do_rgba_to_hsva(bNode *node, float *out, float *in)
{
   rgb_to_hsv(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
   out[3]=in[3];
}

void do_rgba_to_ycca(bNode *node, float *out, float *in)
{
   rgb_to_ycc(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
   out[3]=in[3];
}

void do_yuva_to_rgba(bNode *node, float *out, float *in)
{
   yuv_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
   out[3]=in[3];
}

void do_hsva_to_rgba(bNode *node, float *out, float *in)
{
   hsv_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
   out[3]=in[3];
}

void do_ycca_to_rgba(bNode *node, float *out, float *in)
{
   ycc_to_rgb(in[0],in[1],in[2], &out[0], &out[1], &out[2]);
   out[3]=in[3];
}

void do_copy_rgba(bNode *node, float *out, float *in)
{
   QUATCOPY(out, in);
}

void do_copy_rgb(bNode *node, float *out, float *in)
{
   VECCOPY(out, in);
   out[3]= 1.0f;
}

void do_copy_value(bNode *node, float *out, float *in)
{
   out[0]= in[0];
}

void do_copy_a_rgba(bNode *node, float *out, float *in, float *fac)
{
   VECCOPY(out, in);
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
