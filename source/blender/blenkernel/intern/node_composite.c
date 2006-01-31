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

#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"
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
#include "BLI_threads.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

/* *************************** operations support *************************** */

/* general signal that's in output sockets, and goes over the wires */
typedef struct CompBuf {
	float *rect;
	int x, y;
	short type, malloc;
	rcti disprect;		/* cropped part of image */
	int xof, yof;		/* relative to center of target image */
} CompBuf;

/* defines also used for pixel size */
#define CB_RGBA		4
#define CB_VEC3		3
#define CB_VEC2		2
#define CB_VAL		1

static CompBuf *alloc_compbuf(int sizex, int sizey, int type, int alloc)
{
	CompBuf *cbuf= MEM_callocT(sizeof(CompBuf), "compbuf");
	
	cbuf->x= sizex;
	cbuf->y= sizey;
	cbuf->type= type;
	if(alloc) {
		if(cbuf->type==CB_RGBA)
			cbuf->rect= MEM_mallocT(4*sizeof(float)*sizex*sizey, "compbuf RGBA rect");
		else if(cbuf->type==CB_VEC3)
			cbuf->rect= MEM_mallocT(4*sizeof(float)*sizex*sizey, "compbuf Vector3 rect");
		else if(cbuf->type==CB_VEC2)
			cbuf->rect= MEM_mallocT(4*sizeof(float)*sizex*sizey, "compbuf Vector2 rect");
		else
			cbuf->rect= MEM_mallocT(sizeof(float)*sizex*sizey, "compbuf Fac rect");
		cbuf->malloc= 1;
	}
	cbuf->disprect.xmin= 0;
	cbuf->disprect.ymin= 0;
	cbuf->disprect.xmax= sizex;
	cbuf->disprect.ymax= sizey;
	
	return cbuf;
}

void free_compbuf(CompBuf *cbuf)
{
	if(cbuf->malloc && cbuf->rect)
		MEM_freeT(cbuf->rect);
	MEM_freeT(cbuf);
}

void print_compbuf(char *str, CompBuf *cbuf)
{
	printf("Compbuf %s %d %d %p\n", str, cbuf->x, cbuf->y, cbuf->rect);
	
}



#if 0
/* on first call, disprect should be initialized to 'out', then you can call this on all 'src' images */
static void get_overlap_rct(CompBuf *out, CompBuf *src, rcti *disprect)
{
	rcti rect;
	/* output center is considered (0,0) */
	
	if(src==NULL) return;
	
	/* translate src into output space */
	rect= src->disprect;
	BLI_translate_rcti(&rect, out->xof-src->xof, out->xof-src->xof);
	/* intersect rect with current disprect */
	
	BLI_isect_rcti(&rect, disprect, disprect);
}

static void get_scanline_rcti(CompBuf *out, rcti *disprect, CompBuf *src, rcti *srcrect)
{
	int xof, yof;
	
	/* translate src into output space */
	xof= out->xof-src->xof;
	yof= out->xof-src->xof;
	
	srcrect->xmin= disprect->xmin + xof;
	srcrect->ymin= disprect->ymin + yof;
	srcrect->xmax= disprect->xmax + xof;
	srcrect->ymax= disprect->ymax + yof;
}
#endif

/* Pixel-to-Pixel operation, 1 Image in, 1 out */
static void composit1_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
									  void (*func)(bNode *, float *, float *))
{
	float *outfp, *srcfp, *out_data, *src_data;
	int outx, outy;
	int srcx, srcy;
	int out_pix, out_stride, src_stride, src_pix, x, y;
	
	outx= out->x;
	outy= out->y;
	out_pix= out->type;
	out_stride= out->x;
	out_data= out->rect;
	
	/* handle case when input is constant color */
	if(src_buf==NULL) {
		srcx= outx; srcy= outy;
		src_stride= 0;
		src_pix= 0;
		src_data= src_col;
	}
	else {
		srcx= src_buf->x;
		srcy= src_buf->y;
		src_stride= srcx;
		src_pix= src_buf->type;
		src_data= src_buf->rect;
	}
	
	outx= MIN2(outx, srcx);
	outy= MIN2(outy, srcy);

	for(y=0; y<outy; y++) {
		/* set scanlines on right location */
		srcfp= src_data + src_pix*y*src_stride;
		outfp= out_data + out_pix*y*out_stride;
			
		for(x=0; x<outx; x++) {
			func(node, outfp, srcfp);
			srcfp += src_pix;
			outfp += out_pix;
		}
	}
}

/* Pixel-to-Pixel operation, 2 Images in, 1 out */
static void composit2_pixel_processor(bNode *node, CompBuf *out, CompBuf *src_buf, float *src_col,
									  CompBuf *fac_buf, float *fac, void (*func)(bNode *, float *, float *, float *))
{
	float *outfp, *srcfp, *src_data, *facfp, *fac_data;
	int outx= out->x, outy= out->y;
	int srcx, srcy, facx, facy;
	int out_pix, src_stride, src_pix, fac_stride, fac_pix, x, y;
	
	out_pix= out->type;
	
	/* handle case when input is constant color */
	if(src_buf==NULL) {
		srcx= outx; srcy= outy;
		src_stride= 0;
		src_pix= 0;
		src_data= src_col;
	}
	else {
		srcx= src_buf->x;
		srcy= src_buf->y;
		src_stride= srcx;
		src_pix= src_buf->type;
		src_data= src_buf->rect;
	}
	
	/* factor buf or constant? */
	if(fac_buf==NULL) {
		facx= outx; facy= outy;
		fac_stride= 0;
		fac_pix= 0;
		fac_data= fac;
	}
	else {
		facx= fac_buf->x;
		facy= fac_buf->y;
		fac_stride= facx;
		fac_pix= fac_buf->type;
		fac_data= fac_buf->rect;
	}
	
	if(fac_data==NULL) {
		printf("fac buffer error, node %s\n", node->name);
		return;
	}
	
	facx= MIN2(facx, srcx);
	facy= MIN2(facy, srcy);
	
#if 0	
	if(src_buf) {
		rcti disprect;
		
		disprect= out->disprect;
		get_overlap_rct(out, src_buf, &disprect);
		printf("%s\n", node->name);
		printf("union %d %d %d %d\n", disprect.xmin,disprect.ymin,disprect.xmax,disprect.ymax);
	}
	/* new approach */
	outfp= out->rect_float + src.ymin*outx + ;
	for(y=src.ymin; y<src.ymax; y++) {
		
		/* all operators available */
		if(y>=disp.ymin && y<disp.ymax) {
			srcfp= src_data + (src_stride*(y+scrc.ymin) + src.xmin);
			facfp= fac_data + (fac_stride*(y+fac.ymin) + fac.xmin);
			
			for(x= src.xmin; x<src.xmax; x++) {
				if(x>=disp.xmin && x<disp.xmax) {
					
					srcfp+= src_pix;
					facfp+= fac_pix;
				}
				else {
					/* copy src1 */
				}
			}
		}
		else {
			/* copy src1 */
			srcfp= src_data + (src_stride*(y+scrc.ymin) + src.xmin);
			
			QUATCOPY(outfp, srcfp);
		}
	}	
#endif
	
	outfp= out->rect;
	for(y=0; y<outy; y++) {
		/* set source scanline on right location */
		srcfp= src_data + src_pix*y*src_stride;
		facfp= fac_data + fac_pix*y*fac_stride;
		
		for(x=0; x<outx; x++, outfp+=out_pix) {
			if(x<facx && y<facy)
				func(node, outfp, srcfp, facfp);
			srcfp += src_pix;
			facfp += fac_pix;
		}
	}
}

/* Pixel-to-Pixel operation, 3 Images in, 1 out */
static void composit3_pixel_processor(bNode *node, CompBuf *out, CompBuf *src1_buf, float *src1_col, CompBuf *src2_buf, float *src2_col, 
									  CompBuf *fac_buf, float fac, void (*func)(bNode *, float *, float *, float *, float))
{
	float *outfp, *src1fp, *src2fp, *facfp, *src1_data, *src2_data, *fac_data;
	int outx= out->x, outy= out->y;
	int src1x, src1y, src2x, src2y, facx, facy;
	int src1_stride, src1_pix, src2_stride, src2_pix, fac_stride, fac_pix, x, y;

	/* handle case when input has constant color */
	if(src1_buf==NULL) {
		src1x= outx; src1y= outy;
		src1_stride= 0;
		src1_pix= 0;
		src1_data= src1_col;
	}
	else {
		src1x= src1_buf->x;
		src1y= src1_buf->y;
		src1_stride= src1x;
		src1_pix= src1_buf->type;
		src1_data= src1_buf->rect;
	}
	
	if(src2_buf==NULL) {
		src2x= outx; src2y= outy;
		src2_stride= 0;
		src2_pix= 0;
		src2_data= src2_col;
	}
	else {
		src2x= src2_buf->x;
		src2y= src2_buf->y;
		src2_stride= src2x;
		src2_pix= src2_buf->type;
		src2_data= src2_buf->rect;
	}
	
	/* factor buf or constant? */
	if(fac_buf==NULL) {
		facx= outx; facy= outy;
		fac_stride= 0;
		fac_pix= 0;
		fac_data= &fac;
	}
	else {
		facx= fac_buf->x;
		facy= fac_buf->y;
		fac_stride= facx;
		fac_pix= 1;
		fac_data= fac_buf->rect;
	}
	
	facx= MIN3(facx, src1x, src2x);
	facy= MIN3(facy, src1y, src2y);
	
	outfp= out->rect;
	for(y=0; y<outy; y++) {
		
		/* set source scanlines on right location */
		src1fp= src1_data + src1_pix*y*src1_stride;
		src2fp= src2_data + src2_pix*y*src2_stride;
		facfp= fac_data + y*fac_stride;
		
		for(x=0; x<outx; x++, outfp+=4) {
			if(x<facx && y<facy)
				func(node, outfp, src1fp, src2fp, *facfp);
			src1fp+= src1_pix;
			src2fp+= src2_pix;
			facfp+= fac_pix;
		}
	}
}

/*  */
static CompBuf *alphabuf_from_rgbabuf(CompBuf *cbuf)
{
	CompBuf *valbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1);
	float *valf, *rectf;
	int tot;
	
	valf= valbuf->rect;
	rectf= cbuf->rect + 3;
	for(tot= cbuf->x*cbuf->y; tot>0; tot--, valf++, rectf+=4)
		*valf= *rectf;
	
	return valbuf;
}

static void generate_preview(bNode *node, CompBuf *stackbuf)
{
	bNodePreview *preview= node->preview;
	
	if(preview) {
		ImBuf *ibuf= IMB_allocImBuf(stackbuf->x, stackbuf->y, 32, 0, 0);	/* empty */
		
		if(stackbuf->x > stackbuf->y) {
			preview->xsize= 140;
			preview->ysize= (140*stackbuf->y)/stackbuf->x;
		}
		else {
			preview->ysize= 140;
			preview->xsize= (140*stackbuf->x)/stackbuf->y;
		}
		ibuf->rect_float= stackbuf->rect;
		ibuf= IMB_scalefastImBuf(ibuf, preview->xsize, preview->ysize);
		
		/* this ensures free-imbuf does the right stuff */
		SWAP(float *, ibuf->rect_float, node->preview->rect);
		
		IMB_freeImBuf(ibuf);
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
	{	SOCK_VALUE, 1, "Z",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_copy_rgba(bNode *node, float *out, float *in)
{
	QUATCOPY(out, in);
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
		Image *ima= (Image *)node->id;
		CompBuf *cbuf;
		int rectx, recty;
		
		/* scene size? */
		if(1) {
			RenderData *rd= data;

			/* re-create output, derive size from scene */
			rectx= (rd->size*rd->xsch)/100;
			recty= (rd->size*rd->ysch)/100;
			
			if(ima->ibuf) IMB_freeImBuf(ima->ibuf);
			ima->ibuf= IMB_allocImBuf(rectx, recty, 32, IB_rectfloat, 0); // do alloc
			
			cbuf= alloc_compbuf(rectx, recty, CB_RGBA, 0);	// no alloc
			cbuf->rect= ima->ibuf->rect_float;

			/* when no alpha, we can simply copy */
			if(in[1]->data==NULL)
				composit1_pixel_processor(node, cbuf, in[0]->data, in[0]->vec, do_copy_rgba);
			else
				composit2_pixel_processor(node, cbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba);
			
			if(in[2]->data) {
				CompBuf *zbuf= alloc_compbuf(rectx, recty, CB_VAL, 0);
				addzbuffloatImBuf(ima->ibuf);
				zbuf->rect= ima->ibuf->zbuf_float;
				composit1_pixel_processor(node, zbuf, in[2]->data, in[2]->vec, do_copy_value);
				free_compbuf(zbuf);
			}

			generate_preview(node, cbuf);
			free_compbuf(cbuf);
		}
		else { /* test */
			if(ima->ibuf) IMB_freeImBuf(ima->ibuf);
			ima->ibuf= IMB_allocImBuf(rectx, recty, 32, 0, 0); // do alloc
			ima->ibuf->mall= IB_rectfloat;
			cbuf= in[0]->data;
			ima->ibuf->rect_float= cbuf->rect;
			ima->ibuf->x= cbuf->x;
			ima->ibuf->y= cbuf->y;
			cbuf->rect= NULL;
		}

	}	/* lets make only previews when not done yet, so activating doesnt update */
	else if(in[0]->data && node->preview && node->preview->rect==NULL)
		generate_preview(node, in[0]->data);
}

static bNodeType cmp_node_viewer= {
	/* type code   */	CMP_NODE_VIEWER,
	/* name        */	"Viewer",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_OUTPUT, NODE_PREVIEW,
	/* input sock  */	cmp_node_viewer_in,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_viewer
	
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
			RenderResult *rr= RE_GetResult(RE_GetRender("Render"));
			if(rr) {
				CompBuf *outbuf, *zbuf=NULL;
				
				if(rr->rectf) 
					MEM_freeT(rr->rectf);
				outbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 1);
				
				if(in[1]->data==NULL)
					composit1_pixel_processor(node, outbuf, in[0]->data, in[0]->vec, do_copy_rgba);
				else
					composit2_pixel_processor(node, outbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_copy_a_rgba);
				
				if(in[2]->data) {
					if(rr->rectz) 
						MEM_freeT(rr->rectz);
					zbuf= alloc_compbuf(rr->rectx, rr->recty, CB_VAL, 1);
					composit1_pixel_processor(node, zbuf, in[2]->data, in[2]->vec, do_copy_value);
					rr->rectz= zbuf->rect;
					zbuf->malloc= 0;
					free_compbuf(zbuf);
				}
				generate_preview(node, outbuf);
				
				/* we give outbuf to rr... */
				rr->rectf= outbuf->rect;
				outbuf->malloc= 0;
				free_compbuf(outbuf);
				
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
	{	SOCK_VALUE, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};


static void node_composit_exec_output_file(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	
	if(node->id && (node->flag & NODE_DO_OUTPUT)) {	/* only one works on out */
	}
	else if(in[0]->data)
		generate_preview(node, in[0]->data);
}

static bNodeType cmp_node_output_file= {
	/* type code   */	CMP_NODE_OUTPUT_FILE,
	/* name        */	"File Output",
	/* width+range */	80, 60, 200,
	/* class+opts  */	NODE_CLASS_FILE, NODE_PREVIEW,
	/* input sock  */	cmp_node_output_file_in,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_output_file
	
};

/* **************** IMAGE ******************** */
static bNodeSocketType cmp_node_image_out[]= {
	{	SOCK_RGBA, 0, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static int calcimanr(int cfra, NodeImageAnim *nia)
{
	
	if(nia->frames==0) return nia->nr;
	
	cfra= cfra - nia->sfra;
	
	/* cyclic */
	if(nia->cyclic)
		cfra= (cfra % nia->frames);
	else if(cfra>=nia->frames)
		cfra= nia->frames-1;
	else if(cfra<0)
		cfra= 0;
	
	cfra+= nia->nr;
	
	if(cfra<1) cfra= 1;
	
	return cfra;
}


static void animated_image(bNode *node, int cfra)
{
	Image *ima;
	NodeImageAnim *nia;
	int imanr;
	unsigned short numlen;
	char name[FILE_MAXDIR+FILE_MAXFILE], head[FILE_MAXDIR+FILE_MAXFILE], tail[FILE_MAXDIR+FILE_MAXFILE];
	
	ima= (Image *)node->id;
	nia= node->storage;
	
	if(nia && nia->frames && ima && ima->name) {	/* frames */
		strcpy(name, ima->name);
		
		imanr= calcimanr(cfra, nia);
		if(imanr!=ima->lastframe) {
			ima->lastframe= imanr;
			
			BLI_stringdec(name, head, tail, &numlen);
			BLI_stringenc(name, head, tail, numlen, imanr);
			
			ima= add_image(name);
			
			if(ima) {
				ima->flag |= IMA_FROMANIM;
				if(node->id) node->id->us--;
				node->id= (ID *)ima;
				
				ima->ok= 1;
			}
		}
	}
}


static void node_composit_exec_image(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	RenderData *rd= data;
	
	/* image assigned to output */
	/* stack order input sockets: col, alpha */
	if(node->id) {
		Image *ima;
		CompBuf *stackbuf;
		
		/* animated image? */
		if(node->storage)
			animated_image(node, rd->cfra);
		
		ima= (Image *)node->id;
		
		/* test if image is OK */
		if(ima->ok==0) return;
		
		if(ima->ibuf==NULL) {
			
			load_image(ima, IB_rect, G.sce, rd->cfra);	/* G.sce is current .blend path */
			if(ima->ibuf==NULL) {
				ima->ok= 0;
				return;
			}
		}
		if(ima->ibuf->rect_float==NULL)
			IMB_float_from_rect(ima->ibuf);
		
		/* we put imbuf copy on stack, cbuf knows rect is from other ibuf when freed! */
		stackbuf= alloc_compbuf(ima->ibuf->x, ima->ibuf->y, CB_RGBA, 0);
		stackbuf->rect= ima->ibuf->rect_float;
		
		/* put ibuf on stack */	
		out[0]->data= stackbuf;
		
		if(out[1]->hasoutput)
			out[1]->data= alphabuf_from_rgbabuf(stackbuf);
		
		if(out[2]->hasoutput && ima->ibuf->zbuf_float) {
			CompBuf *zbuf= alloc_compbuf(ima->ibuf->x, ima->ibuf->y, CB_VAL, 0);
			zbuf->rect= ima->ibuf->zbuf_float;
			out[2]->data= zbuf;
		}
		
		generate_preview(node, stackbuf);
	}	
}

/* uses node->storage to indicate animated image */

static bNodeType cmp_node_image= {
	/* type code   */	CMP_NODE_IMAGE,
	/* name        */	"Image",
	/* width+range */	120, 80, 300,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_image_out,
	/* storage     */	"NodeImageAnim",
	/* execfunc    */	node_composit_exec_image
	
};

/* **************** RENDER RESULT ******************** */
static bNodeSocketType cmp_node_rresult_out[]= {
	{	SOCK_RGBA, 0, "Image",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Z",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Vec",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_rresult(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	RenderResult *rr= RE_GetResult(RE_GetRender("Render"));
	
	if(rr) {
		RenderLayer *rl= BLI_findlink(&rr->layers, node->custom1);
		if(rl) {
			CompBuf *stackbuf;
			
			/* we put render rect on stack, cbuf knows rect is from other ibuf when freed! */
			stackbuf= alloc_compbuf(rr->rectx, rr->recty, CB_RGBA, 0);
			stackbuf->rect= rl->rectf;
			
			/* put on stack */	
			out[0]->data= stackbuf;
			
			if(out[1]->hasoutput)
				out[1]->data= alphabuf_from_rgbabuf(stackbuf);
			if(out[2]->hasoutput && rl->rectz) {
				CompBuf *zbuf= alloc_compbuf(rr->rectx, rr->recty, CB_VAL, 0);
				zbuf->rect= rl->rectz;
				out[2]->data= zbuf;
			}
			if(out[3]->hasoutput && rl->rectvec) {
				CompBuf *vecbuf= alloc_compbuf(rr->rectx, rr->recty, CB_VEC2, 0);
				vecbuf->rect= rl->rectvec;
				out[3]->data= vecbuf;
			}
			
			generate_preview(node, stackbuf);
		}
	}	
}

/* custom1 = render layer in use */
static bNodeType cmp_node_rresult= {
	/* type code   */	CMP_NODE_R_RESULT,
	/* name        */	"Render Result",
	/* width+range */	120, 80, 300,
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_PREVIEW|NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rresult_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rresult
	
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

/* generates normal, does dot product */
static void node_composit_exec_normal(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	/* stack order input:  normal */
	/* stack order output: normal, value */
	
	VECCOPY(out[0]->vec, sock->ns.vec);
	/* render normals point inside... the widget points outside */
	out[1]->vec[0]= -INPR(out[0]->vec, in[0]->vec);
}

static bNodeType cmp_node_normal= {
	/* type code   */	CMP_NODE_NORMAL,
	/* name        */	"Normal",
	/* width+range */	100, 60, 200,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_normal_in,
	/* output sock */	cmp_node_normal_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_normal
	
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
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
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
	curvemapping_evaluateRGBF(node->storage, out, in);
	out[3]= in[3];
}

static void do_curves_fac(bNode *node, float *out, float *in, float *fac)
{
	
	if(*fac>=1.0)
		curvemapping_evaluateRGBF(node->storage, out, in);
	else if(*fac<=0.0) {
		VECCOPY(out, in);
	}
	else {
		float col[4], mfac= 1.0f-*fac;
		curvemapping_evaluateRGBF(node->storage, col, in);
		out[0]= mfac*in[0] + *fac*col[0];
		out[1]= mfac*in[1] + *fac*col[1];
		out[2]= mfac*in[2] + *fac*col[2];
	}
	out[3]= in[3];
}

static void node_composit_exec_curve_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order input:  vec */
	/* stack order output: vec */
	
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
		
		curvemapping_premultiply(node->storage, 0);
		if(in[0]->data)
			composit2_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[0]->data, in[0]->vec, do_curves_fac);
		else
			composit1_pixel_processor(node, stackbuf, in[1]->data, NULL, do_curves);
		curvemapping_premultiply(node->storage, 1);
		
		out[0]->data= stackbuf;
	}
	
}

static bNodeType cmp_node_curve_rgb= {
	/* type code   */	CMP_NODE_CURVE_RGB,
	/* name        */	"RGB Curves",
	/* width+range */	200, 140, 320,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
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
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS,
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
	/* class+opts  */	NODE_CLASS_GENERATOR, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	cmp_node_rgb_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rgb
	
};

/* **************** MIX RGB ******************** */
static bNodeSocketType cmp_node_mix_rgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_mix_rgb_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_mix_rgb(bNode *node, float *out, float *in1, float *in2, float fac)
{
	float col[3];
	
	VECCOPY(col, in1);
	ramp_blend(node->custom1, col, col+1, col+2, fac, in2);
	VECCOPY(out, col);
	out[3]= in1[3];
}

static void node_composit_exec_mix_rgb(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: fac, Image, Image */
	/* stack order out: Image */
	float fac= in[0]->vec[0];
	
	CLAMP(fac, 0.0f, 1.0f);
	
	/* input no image? then only color operation */
	if(in[1]->data==NULL && in[2]->data==NULL) {
		do_mix_rgb(node, out[0]->vec, in[1]->vec, in[2]->vec, fac);
	}
	else {
		/* make output size of first available input image */
		CompBuf *cbuf= in[1]->data?in[1]->data:in[2]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit3_pixel_processor(node, stackbuf, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, in[0]->data, fac, do_mix_rgb);
		
		out[0]->data= stackbuf;
	}
}

/* custom1 = mix type */
static bNodeType cmp_node_mix_rgb= {
	/* type code   */	CMP_NODE_MIX_RGB,
	/* name        */	"Mix",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
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
	int rowlen, x, y, c;
	
	rowlen= in->x;
	
	if(in->type==CB_RGBA) {
		
		for(y=2; y<in->y; y++) {
			/* setup rows */
			row1= in->rect + 4*(y-2)*rowlen;
			row2= row1 + 4*rowlen;
			row3= row2 + 4*rowlen;
			fp= out->rect + 4*(y-1)*rowlen + 4;
			
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
	
	for(y=2; y<in->y; y++) {
		/* setup rows */
		row1= in->rect + pixlen*(y-2)*rowlen;
		row2= row1 + pixlen*rowlen;
		row3= row2 + pixlen*rowlen;
		
		fp= out->rect + pixlen*(y-1)*rowlen;
		QUATCOPY(fp, row2);
		fp+= pixlen;
		
		for(x=2; x<rowlen; x++) {
			for(c=0; c<pixlen; c++) {
				fp[0]= mfac*row2[4] + fac*(filter[0]*row1[0] + filter[1]*row1[4] + filter[2]*row1[8] + filter[3]*row2[0] + filter[4]*row2[4] + filter[5]*row2[8] + filter[6]*row3[0] + filter[7]*row3[4] + filter[8]*row3[8]);
				fp++; row1++; row2++; row3++;
			}
		}
	}
}

static float soft[9]= {1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f};

static void node_composit_exec_filter(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	float sharp[9]= {-1,-1,-1,-1,9,-1,-1,-1,-1};
	float laplace[9]= {1/8.0f, -1/8.0f, 1/8.0f, -1/8.0f, 1.0f, -1/8.0f, 1/8.0f, -1/8.0f, 1/8.0f};
	float sobel[9]= {1,2,1,0,0,0,-1,-2,-1};
	float prewitt[9]= {1,1,1,0,0,0,-1,-1,-1};
	float kirsch[9]= {5,5,5,-3,-3,-3,-2,-2,-2};
	float shadow[9]= {1,2,1,0,1,0,-1,-2,-1};
	
	/* stack order in: Image */
	/* stack order out: Image */
	
	if(in[1]->data) {
		/* make output size of first available input image */
		CompBuf *cbuf= in[1]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
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
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
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
	
	if(node->storage) {
		/* input no image? then only color operation */
		if(in[0]->data==NULL) {
			do_colorband(node->storage, in[0]->vec[0], out[0]->vec);
		}
		else {
			/* make output size of input image */
			CompBuf *cbuf= in[0]->data;
			CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
			
			composit1_pixel_processor(node, stackbuf, in[0]->data, NULL, do_colorband_composit);
			
			out[0]->data= stackbuf;
			
			if(out[1]->hasoutput)
				out[1]->data= alphabuf_from_rgbabuf(stackbuf);

		}
	}
}

static bNodeType cmp_node_valtorgb= {
	/* type code   */	CMP_NODE_VALTORGB,
	/* name        */	"ColorRamp",
	/* width+range */	240, 200, 300,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
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
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		out[0]->vec[0]= in[0]->vec[0]*0.35f + in[0]->vec[1]*0.45f + in[0]->vec[2]*0.2f;
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, NULL, do_rgbtobw);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_rgbtobw= {
	/* type code   */	CMP_NODE_RGBTOBW,
	/* name        */	"RGB to BW",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OPERATOR, 0,
	/* input sock  */	cmp_node_rgbtobw_in,
	/* output sock */	cmp_node_rgbtobw_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_rgbtobw
	
};

/* **************** ALPHAOVER ******************** */
static bNodeSocketType cmp_node_alphaover_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_alphaover_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_alphaover(bNode *node, float *out, float *src, float *dest)
{
	float mul= 1.0f - dest[3];
	
	if(mul<=0.0f) {
		QUATCOPY(out, dest);
	}
	else {
		out[0]= (mul*src[0]) + dest[0];
		out[1]= (mul*src[1]) + dest[1];
		out[2]= (mul*src[2]) + dest[2];
		out[3]= (mul*src[3]) + dest[3];
	}	
}

static void node_composit_exec_alphaover(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	/* stack order in: col col */
	/* stack order out: col */
	
	/* input no image? then only color operation */
	if(in[0]->data==NULL) {
		do_alphaover(node, out[0]->vec, in[0]->vec, in[1]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1); // allocs
		
		composit2_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, do_alphaover);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_alphaover= {
	/* type code   */	CMP_NODE_ALPHAOVER,
	/* name        */	"AlphaOver",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_OPERATOR, 0,
	/* input sock  */	cmp_node_alphaover_in,
	/* output sock */	cmp_node_alphaover_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_alphaover
	
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
	/* stack order in: col col */
	/* stack order out: col */
	
	/* input no image? then only value operation */
	if(in[0]->data==NULL) {
		do_map_value(node, out[0]->vec, in[0]->vec);
	}
	else {
		/* make output size of input image */
		CompBuf *cbuf= in[0]->data;
		CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_VAL, 1); // allocs
		
		composit1_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, do_map_value);
		
		out[0]->data= stackbuf;
	}
}

static bNodeType cmp_node_map_value= {
	/* type code   */	CMP_NODE_MAP_VALUE,
	/* name        */	"Map Value",
	/* width+range */	100, 60, 150,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_map_value_in,
	/* output sock */	cmp_node_map_value_out,
	/* storage     */	"TexMapping",
	/* execfunc    */	node_composit_exec_map_value
	
};

/* **************** GAUSS BLUR ******************** */
static bNodeSocketType cmp_node_blur_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Size",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_blur_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static float *make_gausstab(int rad)
{
	float *gausstab, sum, val;
	int i, n;
	
	n = 2 * rad + 1;
	
	gausstab = (float *) MEM_mallocT(n * sizeof(float), "gauss");
	
	sum = 0.0f;
	for (i = -rad; i <= rad; i++) {
		val = exp(-4.0*((float)i*i) / (float) (rad*rad));
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
	
	bloomtab = (float *) MEM_mallocT(n * sizeof(float), "bloom");
	
	for (i = -rad; i <= rad; i++) {
		val = pow(1.0 - fabs((float)i)/((float)rad), 4.0);
		bloomtab[i+rad] = val;
	}
	
	return bloomtab;
}

static void blur_single_image(CompBuf *new, CompBuf *img, float blurx, float blury)
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
	rad = ceil(blurx);
	if(rad>imgx/2)
		rad= imgx/2;
	else if(rad<1) 
		rad= 1;

	gausstab= make_gausstab(rad);
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
	MEM_freeT(gausstab);
	
	rad = ceil(blury);
	if(rad>imgy/2)
		rad= imgy/2;
	else if(rad<1) 
		rad= 1;

	gausstab= make_gausstab(rad);
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
	MEM_freeT(gausstab);
}

/* reference has to be mapped 0-1, and equal in size */
static void bloom_with_reference(CompBuf *new, CompBuf *img, CompBuf *ref, float blurx, float blury)
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
	memset(wbuf->rect, sizeof(float)*imgx*imgy, 0);
	
	/* horizontal */
	radx = ceil(blurx);
	if(radx>imgx/2)
		radx= imgx/2;
	else if(radx<1) 
		radx= 1;
	
	/* vertical */
	rady = ceil(blury);
	if(rady>imgy/2)
		rady= imgy/2;
	else if(rady<1) 
		rady= 1;
	
	x= MAX2(radx, rady);
	maintabs= MEM_mallocT(x*sizeof(void *), "gauss array");
	for(i= 0; i<x; i++)
		maintabs[i]= make_bloomtab(i+1);
		
	/* vars to store before we go */
//	refd= ref->rect;
	src= img->rect;
	
	memset(new->rect, 4*imgx*imgy, 0);
	
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
		MEM_freeT(maintabs[i]);
	MEM_freeT(maintabs);
	
}

/* reference has to be mapped 0-1, and equal in size */
static void blur_with_reference(CompBuf *new, CompBuf *img, CompBuf *ref, float blurx, float blury)
{
	CompBuf *blurbuf;
	register float sum, val;
	float rval, gval, bval, aval, radxf, radyf;
	float **maintabs;
	float *gausstabx, *gausstabcenty;
	float *gausstaby, *gausstabcentx;
	int radx, rady, imgx= img->x, imgy= img->y;
	int x, y;
	int i, j;
	float *src, *dest, *refd, *blurd;

	/* trick is; we blur the reference image... but only works with clipped values*/
	blurbuf= alloc_compbuf(imgx, imgy, CB_VAL, 1);
	blurd= blurbuf->rect;
	refd= ref->rect;
	for(x= imgx*imgy; x>0; x--, refd++, blurd++) {
		if(refd[0]<0.0f) blurd[0]= 0.0f;
		else if(refd[0]>1.0f) blurd[0]= 1.0f;
		else blurd[0]= refd[0];
	}
	
	blur_single_image(blurbuf, blurbuf, blurx, blury);
	
	/* horizontal */
	radx = ceil(blurx);
	if(radx>imgx/2)
		radx= imgx/2;
	else if(radx<1) 
		radx= 1;
	
	/* vertical */
	rady = ceil(blury);
	if(rady>imgy/2)
		rady= imgy/2;
	else if(rady<1) 
		rady= 1;
	
	x= MAX2(radx, rady);
	maintabs= MEM_mallocT(x*sizeof(void *), "gauss array");
	for(i= 0; i<x; i++)
		maintabs[i]= make_gausstab(i+1);
	
	refd= blurbuf->rect;
	dest= new->rect;
	radxf= (float)radx;
	radyf= (float)rady;
	
	for (y = 0; y < imgy; y++) {
		for (x = 0; x < imgx ; x++, dest+=4, refd++) {
			int refradx= (int)(refd[0]*radxf);
			int refrady= (int)(refd[0]*radyf);
			
			if(refradx>radx) refradx= radx;
			else if(refradx<1) refradx= 1;
			if(refrady>rady) refrady= rady;
			else if(refrady<1) refrady= 1;

			if(refradx==1 && refrady==1) {
				src= img->rect + 4*( y*imgx + x);
				QUATCOPY(dest, src);
			}
			else {
				int minxr= x-refradx<0?-x:-refradx;
				int maxxr= x+refradx>imgx?imgx-x:refradx;
				int minyr= y-refrady<0?-y:-refrady;
				int maxyr= y+refrady>imgy?imgy-y:refrady;
	
				float *srcd= img->rect + 4*( (y + minyr)*imgx + x + minxr);
				
				gausstabx= maintabs[refradx-1];
				gausstabcentx= gausstabx+refradx;
				gausstaby= maintabs[refrady-1];
				gausstabcenty= gausstaby+refrady;

				sum= gval = rval= bval= aval= 0.0f;
				
				for (i= minyr; i < maxyr; i++, srcd+= 4*imgx) {
					src= srcd;
					for (j= minxr; j < maxxr; j++, src+=4) {
					
						val= gausstabcenty[i]*gausstabcentx[j];
						sum+= val;
						rval += val * src[0];
						gval += val * src[1];
						bval += val * src[2];
						aval += val * src[3];
					}
				}
				sum= 1.0f/sum;
				dest[0] = rval*sum;
				dest[1] = gval*sum;
				dest[2] = bval*sum;
				dest[3] = aval*sum;
			}
		}
	}
	
	free_compbuf(blurbuf);
	
	x= MAX2(radx, rady);
	for(i= 0; i<x; i++)
		MEM_freeT(maintabs[i]);
	MEM_freeT(maintabs);
	
}



static void node_composit_exec_blur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *img= in[0]->data;
	
	if(img==NULL || out[0]->hasoutput==0)
		return;
	
	/* if fac input, we do it different */
	if(in[1]->data) {
		
		/* test fitness if reference */
		
		/* make output size of input image */
		new= alloc_compbuf(img->x, img->y, CB_RGBA, 1); // allocs
		
		blur_with_reference(new, img, in[1]->data, (float)node->custom1, (float)node->custom2);
		
		out[0]->data= new;
	}
	else {
		if(in[1]->vec[0]==0.0f) {
			/* pass on image */
			new= alloc_compbuf(img->x, img->y, img->type, 0);
			new->rect= img->rect;
		}
		else {
			/* make output size of input image */
			new= alloc_compbuf(img->x, img->y, img->type, 1); // allocs
			if(1)
				blur_single_image(new, img, in[1]->vec[0]*(float)node->custom1, in[1]->vec[0]*(float)node->custom2);
			else	/* bloom experimental... */
				bloom_with_reference(new, img, NULL, in[1]->vec[0]*(float)node->custom1, in[1]->vec[0]*(float)node->custom2);
		}
		out[0]->data= new;
	}
}
	

/* custom1 custom2 = blur filter size */
static bNodeType cmp_node_blur= {
	/* type code   */	CMP_NODE_BLUR,
	/* name        */	"Blur",
	/* width+range */	120, 80, 200,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_blur_in,
	/* output sock */	cmp_node_blur_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_blur
	
};

/* **************** VECTOR BLUR ******************** */
static bNodeSocketType cmp_node_vecblur_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Vec",			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_vecblur_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void fill_rct_in_image(rcti poly, float *rect, int xsize, int ysize, float *col)
{
	float *dest;
	int x, y;
	
	for(y=poly.ymin; y<poly.ymax; y++) {
		for(x=poly.xmin; x<poly.xmax; x++) {
			dest= rect + 4*(xsize*y + x);
			QUATCOPY(dest, col);
		}
	}
	
}


static void node_composit_exec_vecblur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *img= in[0]->data, *vecbuf= in[1]->data, *wbuf;
	float *vect, *dest;
	int x, y;
	
	if(img==NULL || vecbuf==NULL || out[0]->hasoutput==0)
		return;
	if(vecbuf->x!=img->x || vecbuf->y!=img->y) {
		printf("cannot do different sized vecbuf yet\n");
		return;
	}
	if(vecbuf->type!=CB_VEC2) {
		printf("input should be vecbuf\n");
		return;
	}
	
	/* make weights version of vectors */
	wbuf= alloc_compbuf(img->x, img->y, CB_VAL, 1); // allocs
	dest= wbuf->rect;
	vect= vecbuf->rect;
	for(y=0; y<img->y; y++) {
		for(x=0; x<img->x; x++, dest++, vect+=2) {
			*dest= 0.02f*sqrt(vect[0]*vect[0] + vect[1]*vect[1]);
			if(*dest>1.0f) *dest= 1.0f;
		}
	}
	
	/* make output size of input image */
	new= alloc_compbuf(img->x, img->y, CB_RGBA, 1); // allocs
	
	blur_with_reference(new, img, wbuf, 100.0f, 100.0f);
	
	free_compbuf(wbuf);
	out[0]->data= new;
}

static bNodeType cmp_node_vecblur= {
	/* type code   */	CMP_NODE_VECBLUR,
	/* name        */	"Vector Blur",
	/* width+range */	120, 80, 200,
	/* class+opts  */	NODE_CLASS_OPERATOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_vecblur_in,
	/* output sock */	cmp_node_vecblur_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_vecblur
	
};


/* ****************** types array for all shaders ****************** */

bNodeType *node_all_composit[]= {
	&node_group_typeinfo,
	&cmp_node_viewer,
	&cmp_node_composite,
	&cmp_node_output_file,
	&cmp_node_value,
	&cmp_node_rgb,
	&cmp_node_mix_rgb,
	&cmp_node_filter,
	&cmp_node_valtorgb,
	&cmp_node_rgbtobw,
	&cmp_node_normal,
	&cmp_node_curve_vec,
	&cmp_node_curve_rgb,
	&cmp_node_image,
	&cmp_node_rresult,
	&cmp_node_alphaover,
	&cmp_node_blur,
	&cmp_node_vecblur,
	&cmp_node_map_value,
	NULL
};

/* ******************* parse ************ */

/* helper call to detect if theres a render-result node */
int ntreeCompositNeedsRender(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return 1;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_RESULT)
			return 1;
	}
	return 0;
}

/* called from render pipeline, to tag render input and output */
void ntreeCompositTagRender(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if( ELEM(node->type, CMP_NODE_R_RESULT, CMP_NODE_COMPOSITE))
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
			/* no actual test yet... */
			if(node->storage)
				NodeTagChanged(ntree, node);
		}
	}
}
