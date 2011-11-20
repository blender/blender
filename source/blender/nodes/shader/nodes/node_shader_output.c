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

/** \file blender/nodes/shader/nodes/node_shader_output.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** OUTPUT ******************** */
static bNodeSocketTemplate sh_node_output_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};

static void node_shader_exec_output(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
	if(data) {
		ShadeInput *shi= ((ShaderCallData *)data)->shi;
		float col[4];
		
		/* stack order input sockets: col, alpha, normal */
		nodestack_get_vec(col, SOCK_VECTOR, in[0]);
		nodestack_get_vec(col+3, SOCK_FLOAT, in[1]);
		
		if(shi->do_preview) {
			nodeAddToPreview(node, col, shi->xs, shi->ys, shi->do_manage);
			node->lasty= shi->ys;
		}
		
		if(node->flag & NODE_DO_OUTPUT) {
			ShadeResult *shr= ((ShaderCallData *)data)->shr;
			
			copy_v4_v4(shr->combined, col);
			shr->alpha= col[3];
			
			//	copy_v3_v3(shr->nor, in[3]->vec);
		}
	}	
}

static int gpu_shader_output(GPUMaterial *mat, bNode *UNUSED(node), GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *outlink;

	/*if(in[1].hasinput)
		GPU_material_enable_alpha(mat);*/

	GPU_stack_link(mat, "output_node", in, out, &outlink);
	GPU_material_output_link(mat, outlink);

	return 1;
}

void register_node_type_sh_output(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_OUTPUT, "Output", NODE_CLASS_OUTPUT, NODE_PREVIEW);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_output_in, NULL);
	node_type_size(&ntype, 80, 60, 200);
	node_type_exec(&ntype, node_shader_exec_output);
	node_type_gpu(&ntype, gpu_shader_output);

	/* Do not allow muting output node. */
	node_type_mute(&ntype, NULL, NULL);
	node_type_gpu_mute(&ntype, NULL);

	nodeRegisterType(ttype, &ntype);
}
