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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_bricks.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

#include <math.h>

static bNodeSocketTemplate inputs[]= {
	{ SOCK_RGBA,  1, "Bricks 1",    0.596f, 0.282f, 0.0f,  1.0f },
	{ SOCK_RGBA,  1, "Bricks 2",    0.632f, 0.504f, 0.05f, 1.0f },
	{ SOCK_RGBA,  1, "Mortar",      0.0f,   0.0f,   0.0f,  1.0f },
	{ SOCK_FLOAT, 1, "Thickness",   0.02f,  0.0f,   0.0f,  0.0f,  0.0f,    1.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, "Bias",        0.0f,   0.0f,   0.0f,  0.0f, -1.0f,    1.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, "Brick Width", 0.5f,   0.0f,   0.0f,  0.0f,  0.001f, 99.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, "Row Height",  0.25f,  0.0f,   0.0f,  0.0f,  0.001f, 99.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs[]= {
	{ SOCK_RGBA, 0, "Color"},
	{ -1, 0, ""	}
};

static void init(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
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
		copy_v4_v4( out, mortar );
	} else {
		copy_v4_v4( out, bricks1 );
		ramp_blend( MA_RAMP_BLEND, out, tint, bricks2 );
	}
}

static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &colorfn, data);
}

void register_node_type_tex_bricks(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_BRICKS, "Bricks", NODE_CLASS_PATTERN, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_size(&ntype, 150, 60, 150);
	node_type_init(&ntype, init);
	node_type_exec(&ntype, exec);
	
	nodeRegisterType(ttype, &ntype);
}
