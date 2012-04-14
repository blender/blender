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

/** \file blender/nodes/texture/nodes/node_texture_curves.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

/* **************** CURVE Time  ******************** */

/* custom1 = sfra, custom2 = efra */
static bNodeSocketTemplate time_outputs[]= {
	{ SOCK_FLOAT, 0, "Value" },
	{ -1, 0, "" }
};

static void time_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **UNUSED(in), short UNUSED(thread))
{
	/* stack order output: fac */
	float fac= 0.0f;
	
	if (node->custom1 < node->custom2)
		fac = (p->cfra - node->custom1)/(float)(node->custom2-node->custom1);
	
	fac = curvemapping_evaluateF(node->storage, 0, fac);
	out[0] = CLAMPIS(fac, 0.0f, 1.0f);
}

static void time_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &time_colorfn, data);
}


static void time_init(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->custom1= 1;
	node->custom2= 250;
	node->storage= curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_tex_curve_time(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_CURVE_TIME, "Time", NODE_CLASS_INPUT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, time_outputs);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, time_init);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, time_exec);
	
	nodeRegisterType(ttype, &ntype);
}

/* **************** CURVE RGB  ******************** */
static bNodeSocketTemplate rgb_inputs[]= {
	{	SOCK_RGBA, 1, "Color",	0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate rgb_outputs[]= {
	{	SOCK_RGBA, 0, "Color"},
	{	-1, 0, ""	}
};

static void rgb_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float cin[4];
	tex_input_rgba(cin, in[0], p, thread);
	
	curvemapping_evaluateRGBF(node->storage, out, cin);
	out[3] = cin[3];
}

static void rgb_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &rgb_colorfn, data);
}

static void rgb_init(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_tex_curve_rgb(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, TEX_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, rgb_inputs, rgb_outputs);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, rgb_init);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
	node_type_exec(&ntype, rgb_exec);
	
	nodeRegisterType(ttype, &ntype);
}
