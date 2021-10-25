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

/** \file blender/nodes/shader/nodes/node_shader_rgb.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** RGB ******************** */
static bNodeSocketTemplate sh_node_rgb_out[] = {
	{	SOCK_RGBA, 0, N_("Color"), 0.5f, 0.5f, 0.5f, 1.0f},
	{	-1, 0, ""	}
};

static int gpu_shader_rgb(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *vec = GPU_uniform(out[0].vec);
	return GPU_stack_link(mat, "set_rgba", in, out, vec);
}

void register_node_type_sh_rgb(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_RGB, "RGB", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_OLD_SHADING | NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_rgb_out);
	node_type_gpu(&ntype, gpu_shader_rgb);

	nodeRegisterType(&ntype);
}
