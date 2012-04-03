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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/node_texture_util.c
 *  \ingroup nodes
 */

 
/*
	HOW TEXTURE NODES WORK

	In contrast to Shader nodes, which place a color into the output
	stack when executed, Texture nodes place a TexDelegate* there. To
	obtain a color value from this, a node further up the chain reads
	the TexDelegate* from its input stack, and uses tex_call_delegate to
	retrieve the color from the delegate.
 
	comments: (ton)

	This system needs recode, a node system should rely on the stack, and 
	callbacks for nodes only should evaluate own node, not recursively go
	over other previous ones.
*/

#include <assert.h>
#include "node_texture_util.h"

#define PREV_RES 128 /* default preview resolution */

static void tex_call_delegate(TexDelegate *dg, float *out, TexParams *params, short thread)
{
	if (dg->node->need_exec) {
		dg->fn(out, params, dg->node, dg->in, thread);

		if (dg->cdata->do_preview)
			tex_do_preview(dg->node, params->previewco, out);
	}
}

static void tex_input(float *out, int sz, bNodeStack *in, TexParams *params, short thread)
{
	TexDelegate *dg = in->data;
	if (dg) {
		tex_call_delegate(dg, in->vec, params, thread);
	
		if (in->hasoutput && in->sockettype == SOCK_FLOAT)
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
	
	if (in->hasoutput && in->sockettype == SOCK_FLOAT) {
		out[1] = out[2] = out[0];
		out[3] = 1;
	}
	
	if (in->hasoutput && in->sockettype == SOCK_VECTOR) {
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

void params_from_cdata(TexParams *out, TexCallData *in)
{
	out->co = in->co;
	out->dxt = in->dxt;
	out->dyt = in->dyt;
	out->previewco = in->co;
	out->osatex = in->osatex;
	out->cfra = in->cfra;
	out->shi = in->shi;
	out->mtex = in->mtex;
}

void tex_do_preview(bNode *node, float *co, float *col)
{
	bNodePreview *preview= node->preview;

	if (preview) {
		int xs= ((co[0] + 1.0f)*0.5f)*preview->xsize;
		int ys= ((co[1] + 1.0f)*0.5f)*preview->ysize;

		nodeAddToPreview(node, col, xs, ys, 0); /* 0 = no color management */
	}
}

void tex_output(bNode *node, bNodeStack **in, bNodeStack *out, TexFn texfn, TexCallData *cdata)
{
	TexDelegate *dg;
	if (!out->data)
		/* Freed in tex_end_exec (node.c) */
		dg = out->data = MEM_mallocN(sizeof(TexDelegate), "tex delegate");
	else
		dg = out->data;
	
	dg->cdata= cdata;
	dg->fn = texfn;
	dg->node = node;
	memcpy(dg->in, in, MAX_SOCKET * sizeof(bNodeStack*));
	dg->type = out->sockettype;
}

void ntreeTexCheckCyclics(struct bNodeTree *ntree)
{
	bNode *node;
	for (node= ntree->nodes.first; node; node= node->next) {
		
		if (node->type == TEX_NODE_TEXTURE && node->id) {
			/* custom2 stops the node from rendering */
			if (node->custom1) {
				node->custom2 = 1;
				node->custom1 = 0;
			}
			else {
				Tex *tex = (Tex *)node->id;
				
				node->custom2 = 0;
			
				node->custom1 = 1;
				if (tex->use_nodes && tex->nodetree) {
					ntreeTexCheckCyclics(tex->nodetree);
				}
				node->custom1 = 0;
			}
		}

	}
}
