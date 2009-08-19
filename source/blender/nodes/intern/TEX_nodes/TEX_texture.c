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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../TEX_util.h"
#include "RE_shader_ext.h"

static bNodeSocketType inputs[]= {
	{ SOCK_RGBA, 1, "Color1", 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f },
	{ SOCK_RGBA, 1, "Color2", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

static bNodeSocketType outputs[]= {
	{ SOCK_RGBA, 0, "Color", 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	static float red[] = {1,0,0,1};
	static float white[] = {1,1,1,1};
	float *coord = p->coord;
	
	Tex *nodetex = (Tex *)node->id;
	
	if(node->custom2 || node->need_exec==0) {
		/* this node refers to its own texture tree! */
		QUATCOPY(
			out,
			(fabs(coord[0] - coord[1]) < .01) ? white : red 
		);
	}
	else if(nodetex) {
		TexResult texres;
		int textype;
		float nor[] = {0,0,0};
		float col1[4], col2[4];
		
		tex_input_rgba(col1, in[0], p, thread);
		tex_input_rgba(col2, in[1], p, thread);
		
		texres.nor = nor;
		textype = multitex_ext(nodetex, coord, 0, 0, 0, &texres);
		
		if(textype & TEX_RGB) {
			QUATCOPY(out, &texres.tr);
		}
		else {
			QUATCOPY(out, col1);
			ramp_blend(MA_RAMP_BLEND, out, out+1, out+2, texres.tin, col2);
		}
	}
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn);
	
	tex_do_preview(node, out[0], data);
}

bNodeType tex_node_texture= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_TEXTURE,
	/* name        */	"Texture",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	inputs,
	/* output sock */	outputs,
	/* storage     */	"",
	/* execfunc    */	exec,
	/* butfunc     */	NULL,
	/* initfunc        */   NULL,
	/* freestoragefunc */   NULL,
	/* copystoragefunc */   NULL, 
	/* id          */	NULL
	
};

