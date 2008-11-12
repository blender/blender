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
*/

#include <assert.h>
#include "TEX_util.h"

#define PREV_RES 128 /* default preview resolution */

void tex_call_delegate(TexDelegate *dg, float *out, float *coord, short thread)
{
	dg->fn(out, coord, dg->node, dg->in, thread);
}

void tex_input_vec(float *out, bNodeStack *in, float *coord, short thread)
{
	TexDelegate *dg = in->data;
	if(dg) {
		tex_call_delegate(dg, out, coord, thread);
	
		if(in->hasoutput && in->sockettype == SOCK_VALUE) {
			out[1] = out[2] = out[0];
			out[3] = 1;
		}
	}
	else {
		QUATCOPY(out, in->vec);
	}
}

void tex_input_rgba(float *out, bNodeStack *in, float *coord, short thread)
{
	tex_input_vec(out, in, coord, thread);
	
	if(in->hasoutput && in->sockettype == SOCK_VECTOR) {
		out[0] = out[0] * .5f + .5f;
		out[1] = out[1] * .5f + .5f;
		out[2] = out[2] * .5f + .5f;
		out[3] = 1;
	}
}

float tex_input_value(bNodeStack *in, float *coord, short thread)
{
	float out[4];
	tex_input_vec(out, in, coord, thread);
	return out[0];
}

static void init_preview(bNode *node)
{
	int xsize = node->prvr.xmax - node->prvr.xmin;
	int ysize = node->prvr.ymax - node->prvr.ymin;
	
	if(xsize == 0) {
		xsize = PREV_RES;
		ysize = PREV_RES;
	}
	
	if(node->preview==NULL)
		node->preview= MEM_callocN(sizeof(bNodePreview), "node preview");
	
	if(node->preview->rect)
		if(node->preview->xsize!=xsize && node->preview->ysize!=ysize) {
			MEM_freeN(node->preview->rect);
			node->preview->rect= NULL;
		}
	
	if(node->preview->rect==NULL) {
		node->preview->rect= MEM_callocN(4*xsize + xsize*ysize*sizeof(float)*4, "node preview rect");
		node->preview->xsize= xsize;
		node->preview->ysize= ysize;
	}
}

void tex_do_preview(bNode *node, bNodeStack *ns, TexCallData *cdata)
{
	int x, y;
	float *result;
	bNodePreview *preview;
	
	if(!cdata->do_preview)
		return;
	
	if(!(node->typeinfo->flag & NODE_PREVIEW))
		return;
	
	init_preview(node);
	
	preview = node->preview;
	
	for(x=0; x<preview->xsize; x++)
	for(y=0; y<preview->ysize; y++)
	{
		cdata->coord[0] = ((float) x / preview->xsize) * 2 - 1;
		cdata->coord[1] = ((float) y / preview->ysize) * 2 - 1;
		
		result = preview->rect + 4 * (preview->xsize*y + x);
		
		tex_input_rgba(result, ns, cdata->coord, cdata->thread);
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

void ntreeTexExecTree(bNodeTree *nodes, TexResult *texres, float *coord, char do_preview, short thread, Tex *tex, short which_output)
{
	TexResult dummy_texres;
	TexCallData data;
	
	if(!texres) texres = &dummy_texres;
	data.coord = coord;
	data.target = texres;
	data.do_preview = do_preview;
	data.thread = thread;
	data.which_output = which_output;
	
	ntreeExecTree(nodes, &data, thread);
}

void ntreeTexUpdatePreviews(bNodeTree* nodetree)
{
	Tex *tex;
	float coord[] = {0,0,0};
	TexResult dummy_texres;
	
	for(tex= G.main->tex.first; tex; tex= tex->id.next)
		if(tex->nodetree == nodetree) break;
	if(!tex) return;
	
	dummy_texres.nor = 0;
	
	ntreeBeginExecTree(nodetree);
	ntreeTexExecTree(nodetree, &dummy_texres, coord, 1, 0, tex, 0);
	ntreeEndExecTree(nodetree);
	
	BIF_preview_changed(ID_TE);
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

void ntreeTexAssignIndex(struct bNodeTree *ntree, struct bNode *node)
{
	bNode *tnode;
	int index = 0;
	
	check_index:
	for(tnode= ntree->nodes.first; tnode; tnode= tnode->next)
		if(tnode->type == TEX_NODE_OUTPUT && tnode != node)
			if(tnode->custom1 == index) {
				index ++;
				goto check_index;
			}
			
	node->custom1 = index;
}

