/**
 * $Id: SHD_output.c 32517 2010-10-16 14:32:17Z campbellbarton $
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketType sh_node_tex_distnoise_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, SOCK_NO_VALUE},
	{	SOCK_VALUE, 1, "Size",			1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	SOCK_VALUE, 1, "Distortion",	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_tex_distnoise_out[]= {
	{	SOCK_VALUE, 0, "Fac",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_distorted_noise(bNode *node)
{
	NodeTexDistortedNoise *tex = MEM_callocN(sizeof(NodeTexDistortedNoise), "NodeTexDistortedNoise");
	tex->basis = SHD_NOISE_PERLIN;
	tex->distortion_basis = SHD_NOISE_PERLIN;

	node->storage = tex;
}

static void node_shader_exec_tex_distnoise(void *data, bNode *node, bNodeStack **in, bNodeStack **UNUSED(out))
{
}

/* node type definition */
void register_node_type_sh_tex_distnoise(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_DISTNOISE, "Distorted Noise Texture", NODE_CLASS_TEXTURE, 0,
		sh_node_tex_distnoise_in, sh_node_tex_distnoise_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_distorted_noise);
	node_type_storage(&ntype, "NodeTexDistortedNoise", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_distnoise);
	node_type_gpu(&ntype, NULL);

	nodeRegisterType(lb, &ntype);
};

