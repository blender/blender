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
 * Contributor(s): Bob Holcomb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_levels.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** LEVELS ******************** */
static bNodeSocketTemplate cmp_node_view_levels_in[]= {
	{	SOCK_RGBA, 1, N_("Image"), 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_view_levels_out[]={
	{SOCK_FLOAT, 0, N_("Mean")},
	{SOCK_FLOAT, 0, N_("Std Dev")},
	{-1, 0, ""}
};

static void fill_bins(bNode* node, CompBuf* in, int* bins)
{
	float value[4];
	int ivalue=0;
	int x, y;

	/*fill bins */
	for (y=0; y<in->y; y++) {
		for (x=0; x<in->x; x++) {

			/* get the pixel */
			qd_getPixel(in, x, y, value);

			if (value[3] > 0.0f) { /* don't count transparent pixels */
				switch (node->custom1) {
					case 1: { /* all colors */
						value[0] = rgb_to_bw(value);
						value[0]=value[0]*255; /* scale to 0-255 range */
						ivalue=(int)value[0];
						break;
					}
					case 2: { /* red channel */
						value[0]=value[0]*255; /* scale to 0-255 range */
						ivalue=(int)value[0];
						break;
					}
					case 3:  { /* green channel */
						value[1]=value[1]*255; /* scale to 0-255 range */
						ivalue=(int)value[1];
						break;
					}
					case 4: /*blue channel */
					{
						value[2]=value[2]*255; /* scale to 0-255 range */
						ivalue=(int)value[2];
						break;
					}
					case 5: /* luminence */
					{
						rgb_to_yuv(value[0], value[1], value[2], &value[0], &value[1], &value[2]);
						value[0]=value[0]*255; /* scale to 0-255 range */
						ivalue=(int)value[0];
						break;
					}
				} /*end switch */

				/*clip*/
				if (ivalue<0) ivalue=0;
				if (ivalue>255) ivalue=255;

				/*put in the correct bin*/
				bins[ivalue]+=1;
			} /*end if alpha */
		}
	}	
}

static float brightness_mean(bNode* node, CompBuf* in)
{
	float sum=0.0;
	int numPixels=0.0;
	int x, y;
	float value[4];

	for (x=0; x< in->x; x++) {
		for (y=0; y < in->y; y++) {
			
			/* get the pixel */
			qd_getPixel(in, x, y, value);

			if (value[3] > 0.0f) { /* don't count transparent pixels */
				numPixels++;
				switch (node->custom1) {
				case 1:
					{
						value[0] = rgb_to_bw(value);
						sum+=value[0];
						break;
					}
				case 2:
					{
						sum+=value[0];
						break;
					}
				case 3:
					{
						sum+=value[1];
						break;
					}
				case 4:
					{
						sum+=value[2];
						break;
					}
				case 5:
					{
						rgb_to_yuv(value[0], value[1], value[2], &value[0], &value[1], &value[2]);
						sum+=value[0];
						break;
					}
				}
			}
		}
	}

	return sum/numPixels;
}

static float brightness_standard_deviation(bNode* node, CompBuf* in, float mean)
{
	float sum=0.0;
	int numPixels=0.0;
	int x, y;
	float value[4];

	for (x=0; x< in->x; x++) {
		for (y=0; y < in->y; y++) {
			
			/* get the pixel */
			qd_getPixel(in, x, y, value);

			if (value[3] > 0.0f) { /* don't count transparent pixels */
				numPixels++;
				switch (node->custom1) {
				case 1:
					{
						value[0] = rgb_to_bw(value);
						sum+=(value[0]-mean)*(value[0]-mean);
						break;
					}
				case 2:
					{
						sum+=value[0];
						sum+=(value[0]-mean)*(value[0]-mean);
						break;
					}
				case 3:
					{
						sum+=value[1];
						sum+=(value[1]-mean)*(value[1]-mean);
						break;
					}
				case 4:
					{
						sum+=value[2];
						sum+=(value[2]-mean)*(value[2]-mean);
						break;
					}
				case 5:
					{
						rgb_to_yuv(value[0], value[1], value[2], &value[0], &value[1], &value[2]);
						sum+=(value[0]-mean)*(value[0]-mean);
						break;
					}
				}
			}
		}
	}


	return sqrt(sum/(float)(numPixels-1));
}

static void draw_histogram(bNode *node, CompBuf *out, int* bins)
{
	int x, y;
	float color[4]; 
	float value;
	int max;

	/* find max value */
	max=0;
	for (x=0; x<256; x++) {
		if (bins[x]>max) max=bins[x];
	}

	/*draw histogram in buffer */
	for (x=0; x<out->x; x++) {
		for (y=0;y<out->y; y++) {

			/* get normalized value (0..255) */
			value=((float)bins[x]/(float)max)*255.0f;

			if (y < (int)value) { /*if the y value is below the height of the bar for this line then draw with the color */
				switch (node->custom1) {
					case 1: { /* draw in black */
						color[0]=0.0; color[1]=0.0; color[2]=0.0; color[3]=1.0;
						break;
					}
					case 2: { /* draw in red */
						color[0]=1.0; color[1]=0.0; color[2]=0.0; color[3]=1.0;
						break;
					}
					case 3: { /* draw in green */
						color[0]=0.0; color[1]=1.0; color[2]=0.0; color[3]=1.0;
						break;
					}
					case 4: { /* draw in blue */
						color[0]=0.0; color[1]=0.0; color[2]=1.0; color[3]=1.0;
						break;
					}
					case 5: { /* draw in white */
						color[0]=1.0; color[1]=1.0; color[2]=1.0; color[3]=1.0;
						break;
					}
				}
			}
			else {
				color[0]=0.8; color[1]=0.8; color[2]=0.8; color[3]=1.0;
			}

			/* set the color */
			qd_setPixel(out, x, y, color);
		}
	}
}

static void node_composit_exec_view_levels(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf* cbuf;
	CompBuf* histogram;
	float mean, std_dev;
	int bins[256];
	int x;

	if (in[0]->hasinput==0)  return;
	if (in[0]->data==NULL) return;

	histogram=alloc_compbuf(256, 256, CB_RGBA, 1);	
	cbuf=typecheck_compbuf(in[0]->data, CB_RGBA);	
		
	/*initalize bins*/
	for (x=0; x<256; x++) {
		bins[x]=0;
	}
	
	/*fill bins */
	fill_bins(node, in[0]->data, bins);

	/* draw the histogram chart */
	draw_histogram(node, histogram, bins);

	/* calculate the average brightness and contrast */
	mean=brightness_mean(node, in[0]->data);
	std_dev=brightness_standard_deviation(node, in[0]->data, mean);

	/*  Printf debuging ;) */
#if 0
	printf("Mean: %f\n", mean);
	printf("Std Dev: %f\n", std_dev);
#endif

	if (out[0]->hasoutput)
			out[0]->vec[0]= mean;
	if (out[1]->hasoutput)
			out[1]->vec[0]= std_dev;

	generate_preview(data, node, histogram);

	if (cbuf!=in[0]->data)
		free_compbuf(cbuf);
	free_compbuf(histogram);
}

static void node_composit_init_view_levels(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->custom1=1; /*All channels*/
}

void register_node_type_cmp_view_levels(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_VIEW_LEVELS, "Levels", NODE_CLASS_OUTPUT, NODE_OPTIONS|NODE_PREVIEW);
	node_type_socket_templates(&ntype, cmp_node_view_levels_in, cmp_node_view_levels_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_view_levels);
	node_type_storage(&ntype, "ImageUser", NULL, NULL);
	node_type_exec(&ntype, node_composit_exec_view_levels);

	nodeRegisterType(ttype, &ntype);
}
