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

#include "../node_shader_util.h"
#include "RE_shader_ext.h"

static bNodeSocketTemplate outputs[] = {
	{ SOCK_FLOAT,  0, "Index" },
	{ SOCK_FLOAT,  0, "Random" },
	{ SOCK_FLOAT,  0, "Age" },
	{ SOCK_FLOAT,  0, "Lifetime" },
	{ SOCK_VECTOR,  0, "Location" },
#if 0	/* quaternion sockets not yet supported */
	{ SOCK_QUATERNION,  0, "Rotation" },
#endif
	{ SOCK_FLOAT,  0, "Size" },
	{ SOCK_VECTOR,  0, "Velocity" },
	{ SOCK_VECTOR,  0, "Angular Velocity" },
	{ -1, 0, "" }
};
static void node_shader_exec_particle_info(void *UNUSED(data), int UNUSED(thread), bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), bNodeStack **UNUSED(in), bNodeStack **UNUSED(out))
{
}

static int gpu_shader_particle_info(GPUMaterial *mat, bNode *node, bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{

	return GPU_stack_link(mat, node, "particle_info", in, out,
	                      GPU_builtin(GPU_PARTICLE_SCALAR_PROPS),
	                      GPU_builtin(GPU_PARTICLE_LOCATION),
	                      GPU_builtin(GPU_PARTICLE_VELOCITY),
	                      GPU_builtin(GPU_PARTICLE_ANG_VELOCITY));
}

/* node type definition */
void register_node_type_sh_particle_info(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_PARTICLE_INFO, "Particle Info", NODE_CLASS_INPUT, 0);
	node_type_socket_templates(&ntype, NULL, outputs);
	node_type_exec(&ntype, NULL, NULL, node_shader_exec_particle_info);
	node_type_gpu(&ntype, gpu_shader_particle_info);

	nodeRegisterType(&ntype);
}
