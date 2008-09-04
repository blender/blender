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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** VALUE ******************** */
static bNodeSocketType sh_node_value_out[]= {
	{	SOCK_VALUE, 0, "Value",		0.5f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_value(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock= node->outputs.first;
	
	out[0]->vec[0]= sock->ns.vec[0];
}

static int gpu_shader_value(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	bNodeSocket *sock= node->outputs.first;
	GPUNodeLink *vec = GPU_uniform(sock->ns.vec);

	return GPU_stack_link(mat, "set_value", in, out, vec);
}

bNodeType sh_node_value= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_VALUE,
	/* name        */	"Value",
	/* width+range */	80, 50, 120,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	sh_node_value_out,
	/* storage     */	"", 
	/* execfunc    */	node_shader_exec_value,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_value
	
};

