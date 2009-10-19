/**
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
/*
	HOW TEXTURE NODES WORK

	In contrast to Shader nodes, which place a colour into the output
	stack when executed, Texture nodes place a TexDelegate* there. To
	obtain a colour value from this, a node further up the chain reads
	the TexDelegate* from its input stack, and uses tex_call_delegate to
	retrieve the colour from the delegate.
 
    comments: (ton)
    
    This system needs recode, a node system should rely on the stack, and 
	callbacks for nodes only should evaluate own node, not recursively go
    over other previous ones.
*/

#include <assert.h>
#include "TEX_util.h"

#define PREV_RES 128 /* default preview resolution */

int preview_flag = 0;

void tex_call_delegate(TexDelegate *dg, float *out, TexParams *params, short thread)
{
	if(dg->node->need_exec)
		dg->fn(out, params, dg->node, dg->in, thread);
}

void tex_input(float *out, int sz, bNodeStack *in, TexParams *params, short thread)
{
	TexDelegate *dg = in->data;
	if(dg) {
		tex_call_delegate(dg, in->vec, params, thread);
	
		if(in->hasoutput && in->sockettype == SOCK_VALUE)
			in->vec[1] = in->vec[2] = in->vec[0];
	}
	memcpy(out, in->vec, sz * sizeof(float));
}

void tex_input_vec(float *out, bNodeStack *in, TexParams *params, short thread)
{
	tex_input(out, 3, in, params, thread);
}

void tex_input_rgba(float *out, bNodeStack *in, TexParams *params, short thread)
{
	tex_input(out, 4, in, params, thread);
	
	if(in->hasoutput && in->sockettype == SOCK_VALUE)
	{
		out[1] = out[2] = out[0];
		out[3] = 1;
	}
	
	if(in->hasoutput && in->sockettype == SOCK_VECTOR) {
		out[0] = out[0] * .5f + .5f;
		out[1] = out[1] * .5f + .5f;
		out[2] = out[2] * .5f + .5f;
		out[3] = 1;
	}
}

float tex_input_value(bNodeStack *in, TexParams *params, short thread)
{
	float out[4];
	tex_input_vec(out, in, params, thread);
	return out[0];
}

static void init_preview(bNode *node)
{
	int xsize = (int)(node->prvr.xmax - node->prvr.xmin);
	int ysize = (int)(node->prvr.ymax - node->prvr.ymin);
	
	if(xsize == 0) {
		xsize = PREV_RES;
		ysize = PREV_RES;
	}
	
	if(node->preview==NULL)
		node->preview= MEM_callocN(sizeof(bNodePreview), "node preview");
	
	if(node->preview->rect==NULL) {
		node->preview->rect= MEM_callocN(4*xsize + xsize*ysize*sizeof(float)*4, "node preview rect");
		node->preview->xsize= xsize;
		node->preview->ysize= ysize;
	}
}

void params_from_cdata(TexParams *out, TexCallData *in)
{
	out->coord = in->coord;
	out->dxt = in->dxt;
	out->dyt = in->dyt;
	out->cfra = in->cfra;
}

void tex_do_preview(bNode *node, bNodeStack *ns, TexCallData *cdata)
{
	int x, y;
	float *result;
	bNodePreview *preview;
	float coord[3] = {0, 0, 0};
	TexParams params;
	int resolution;
	int xsize, ysize;
	
	if(!cdata->do_preview)
		return;
	
	if(!(node->typeinfo->flag & NODE_PREVIEW))
		return;
	
	init_preview(node);
	
	preview = node->preview;
	xsize = preview->xsize;
	ysize = preview->ysize;
	
	params.dxt = 0;
	params.dyt = 0;
	params.cfra = cdata->cfra;
	params.coord = coord;
	
	resolution = (xsize < ysize) ? xsize : ysize;
	
	for(x=0; x<xsize; x++)
	for(y=0; y<ysize; y++)
	{
		params.coord[0] = ((float) x / resolution) * 2 - 1;
		params.coord[1] = ((float) y / resolution) * 2 - 1;
		
		result = preview->rect + 4 * (xsize*y + x);
		
		tex_input_rgba(result, ns, &params, cdata->thread);
	}
}

void tex_output(bNode *node, bNodeStack **in, bNodeStack *out, TexFn texfn)
{
	TexDelegate *dg;
	if(!out->data)
		/* Freed in tex_end_exec (node.c) */
		dg = out->data = MEM_mallocN(sizeof(TexDelegate), "tex delegate");
	else
		dg = out->data;
	
	
	dg->fn = texfn;
	dg->node = node;
	memcpy(dg->in, in, MAX_SOCKET * sizeof(bNodeStack*));
	dg->type = out->sockettype;
}

void ntreeTexCheckCyclics(struct bNodeTree *ntree)
{
	bNode *node;
	for(node= ntree->nodes.first; node; node= node->next) {
		
		if(node->type == TEX_NODE_TEXTURE && node->id)
		{
			/* custom2 stops the node from rendering */
			if(node->custom1) {
				node->custom2 = 1;
				node->custom1 = 0;
			} else {
				Tex *tex = (Tex *)node->id;
				
				node->custom2 = 0;
			
				node->custom1 = 1;
				if(tex->use_nodes && tex->nodetree) {
					ntreeTexCheckCyclics(tex->nodetree);
				}
				node->custom1 = 0;
			}
		}

	}
}

void ntreeTexExecTree(
	bNodeTree *nodes,
	TexResult *texres,
	float *coord,
	float *dxt, float *dyt,
	short thread, 
	Tex *tex, 
	short which_output, 
	int cfra
){
	TexResult dummy_texres;
	TexCallData data;
	
	/* 0 means don't care, so just use first */
	if(which_output == 0)
		which_output = 1;
	
	if(!texres) texres = &dummy_texres;
	data.coord = coord;
	data.dxt = dxt;
	data.dyt = dyt;
	data.target = texres;
	data.do_preview = preview_flag;
	data.thread = thread;
	data.which_output = which_output;
	data.cfra= cfra;
	
	preview_flag = 0;
	
	ntreeExecTree(nodes, &data, thread);
}

void ntreeTexSetPreviewFlag(int doit)
{
	preview_flag = doit;
}

char* ntreeTexOutputMenu(bNodeTree *ntree)
{
	bNode *node;
	int len = 1;
	char *str;
	char ctrl[4];
	int index = 0;
	
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->type == TEX_NODE_OUTPUT) {
			len += strlen( 
				((TexNodeOutput*)node->storage)->name
			) + strlen(" %xNNN|");
			index ++;
			
			if(node->custom1 > 999) {
				printf("Error: too many outputs");
				break;
			}
		}
			
	str = malloc(len * sizeof(char));
	*str = 0;

	for(node= ntree->nodes.first; node; node= node->next)
		if(node->type == TEX_NODE_OUTPUT) {
			strcat(str, ((TexNodeOutput*)node->storage)->name);
			strcat(str, " %x");
			
			sprintf(ctrl, "%d", node->custom1);
			strcat(str, ctrl);
			
			if(--index)
				strcat(str, "|");
			else
				break;
		}
	
	return str;
}

