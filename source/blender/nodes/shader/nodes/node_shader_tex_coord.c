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

#include "DNA_customdata_types.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_coord_out[] = {
	{	SOCK_VECTOR, 0, N_("Generated"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, N_("Normal"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, N_("UV"),				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, N_("Object"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, N_("Camera"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, N_("Window"),			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, N_("Reflection"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static int node_shader_gpu_tex_coord(GPUMaterial *mat, bNode *UNUSED(node), bNodeExecData *UNUSED(execdata), GPUNodeStack *in, GPUNodeStack *out)
{
	GPUNodeLink *orco = GPU_attribute(CD_ORCO, "");
	GPUNodeLink *mtface = GPU_attribute(CD_MTFACE, "");
	GPUMatType type = GPU_Material_get_type(mat);
	
	if (type == GPU_MATERIAL_TYPE_MESH) {
		return GPU_stack_link(mat, "node_tex_coord", in, out,
		                      GPU_builtin(GPU_VIEW_POSITION), GPU_builtin(GPU_VIEW_NORMAL),
		                      GPU_builtin(GPU_INVERSE_VIEW_MATRIX), GPU_builtin(GPU_INVERSE_OBJECT_MATRIX), orco, mtface);
	}
	else {
		return GPU_stack_link(mat, "node_tex_coord_background", in, out,
		                      GPU_builtin(GPU_VIEW_POSITION), GPU_builtin(GPU_VIEW_NORMAL),
		                      GPU_builtin(GPU_INVERSE_VIEW_MATRIX), GPU_builtin(GPU_INVERSE_OBJECT_MATRIX), orco, mtface);		
	}
}

/* node type definition */
void register_node_type_sh_tex_coord(void)
{
	static bNodeType ntype;

	sh_node_type_base(&ntype, SH_NODE_TEX_COORD, "Texture Coordinate", NODE_CLASS_INPUT, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, NULL, sh_node_tex_coord_out);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_gpu(&ntype, node_shader_gpu_tex_coord);

	nodeRegisterType(&ntype);
}
