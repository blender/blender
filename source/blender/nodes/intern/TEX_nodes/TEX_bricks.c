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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include <math.h>

static bNodeSocketType inputs[]= {
	{ SOCK_RGBA,  1, "Bricks 1",    0.596f, 0.282f, 0.0f,  1.0f,  0.0f,    1.0f },
	{ SOCK_RGBA,  1, "Bricks 2",    0.632f, 0.504f, 0.05f, 1.0f,  0.0f,    1.0f },
	{ SOCK_RGBA,  1, "Mortar",      0.0f,   0.0f,   0.0f,  1.0f,  0.0f,    1.0f },
	{ SOCK_VALUE, 1, "Thickness",   0.02f,  0.0f,   0.0f,  0.0f,  0.0f,    1.0f },
	{ SOCK_VALUE, 1, "Bias",        0.0f,   0.0f,   0.0f,  0.0f, -1.0f,    1.0f },
	{ SOCK_VALUE, 1, "Brick Width", 0.5f,   0.0f,   0.0f,  0.0f,  0.001f, 99.0f },
	{ SOCK_VALUE, 1, "Row Height",  0.25f,  0.0f,   0.0f,  0.0f,  0.001f, 99.0f },
	{ -1, 0, "" }
};
static bNodeSocketType outputs[]= {
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ -1, 0, ""	}
};

static void init(bNode *node) {
	node->custom3 = 0.5; /* offset */
	node->custom4 = 1.0; /* squash */
}

static float noise(int n) /* fast integer noise */
{
	int nn;
	n = (n >> 13) ^ n;
	nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 0.5f * ((float)nn / 1073741824.0f);
}

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float *co = p->co;
	
	float x = co[0];
	float y = co[1];
	
	int bricknum, rownum;
	float offset = 0;
	float ins_x, ins_y;
	float tint;
	
	float bricks1[4];
	float bricks2[4];
	float mortar[4];
	
	float mortar_thickness = tex_input_value(in[3], p, thread);
	float bias             = tex_input_value(in[4], p, thread);
	float brick_width      = tex_input_value(in[5], p, thread);
	float row_height       = tex_input_value(in[6], p, thread);
	
	tex_input_rgba(bricks1, in[0], p, thread);
	tex_input_rgba(bricks2, in[1], p, thread);
	tex_input_rgba(mortar,  in[2], p, thread);
	
	rownum = (int)floor(y / row_height);
	
	if( node->custom1 && node->custom2 ) {
		brick_width *= ((int)(rownum) % node->custom2 ) ? 1.0f : node->custom4;      /* squash */
		offset = ((int)(rownum) % node->custom1 ) ? 0 : (brick_width*node->custom3); /* offset */
	}
	
	bricknum = (int)floor((x+offset) / brick_width);
	
	ins_x = (x+offset) - brick_width*bricknum;
	ins_y = y - row_height*rownum;
	
	tint = noise((rownum << 16) + (bricknum & 0xFFFF)) + bias;
	CLAMP(tint,0.0f,1.0f);
	
	if( ins_x < mortar_thickness || ins_y < mortar_thickness ||
		ins_x > (brick_width - mortar_thickness) ||
		ins_y > (row_height - mortar_thickness) ) {
		QUATCOPY( out, mortar );
	} else {
		QUATCOPY( out, bricks1 );
		ramp_blend( MA_RAMP_BLEND, out, out+1, out+2, tint, bricks2 );
	}
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn, data);
}

bNodeType tex_node_bricks= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_BRICKS,
	/* name        */	"Bricks",
	/* width+range */	150, 60, 150,
	/* class+opts  */	NODE_CLASS_PATTERN, NODE_OPTIONS | NODE_PREVIEW,
	/* input sock  */	inputs,
	/* output sock */	outputs,
	/* storage     */	"", 
	/* execfunc    */	exec,
	/* butfunc     */	NULL,
	/* initfunc    */	init,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
	
};
