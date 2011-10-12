/**
 * $Id: node_shader_output.c 32517 2010-10-16 14:32:17Z campbellbarton $
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

#include "../node_shader_util.h"

static void magic(float rgb[3], float p[3], int n, float turbulence)
{
	float turb = turbulence/5.0f;

	float x = sinf((p[0] + p[1] + p[2])*5.0f);
	float y = cosf((-p[0] + p[1] - p[2])*5.0f);
	float z = -cosf((-p[0] - p[1] + p[2])*5.0f);

	if(n > 0) {
		x *= turb;
		y *= turb;
		z *= turb;
		y = -cosf(x-y+z);
		y *= turb;

		if(n > 1) {
			x= cosf(x-y-z);
			x *= turb;

			if(n > 2) {
				z= sinf(-x-y-z);
				z *= turb;

				if(n > 3) {
					x= -cosf(-x+y-z);
					x *= turb;

					if(n > 4) {
						y= -sinf(-x+y+z);
						y *= turb;

						if(n > 5) {
							y= -cosf(-x+y+z);
							y *= turb;

							if(n > 6) {
								x= cosf(x+y+z);
								x *= turb;

								if(n > 7) {
									z= sinf(x+y-z);
									z *= turb;

									if(n > 8) {
										x= -cosf(-x-y+z);
										x *= turb;

										if(n > 9) {
											y= -sinf(x-y+z);
											y *= turb;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(turb != 0.0f) {
		turb *= 2.0f;
		x /= turb;
		y /= turb;
		z /= turb;
	}

	rgb[0]= 0.5f - x;
	rgb[1]= 0.5f - y;
	rgb[2]= 0.5f - z;
}

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_magic_in[]= {
	{	SOCK_VECTOR, 1, "Vector",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, "Turbulence",	5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_magic_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_magic(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexMagic *tex = MEM_callocN(sizeof(NodeTexMagic), "NodeTexMagic");
	tex->depth = 2;

	node->storage = tex;
}

static void node_shader_exec_tex_magic(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	ShaderCallData *scd= (ShaderCallData*)data;
	NodeTexMagic *tex= (NodeTexMagic*)node->storage;
	bNodeSocket *vecsock = node->inputs.first;
	float vec[3], turbulence;
	
	if(vecsock->link)
		nodestack_get_vec(vec, SOCK_VECTOR, in[0]);
	else
		copy_v3_v3(vec, scd->co);

	nodestack_get_vec(&turbulence, SOCK_FLOAT, in[1]);

	magic(out[0]->vec, vec, tex->depth, turbulence);
}

static int node_shader_gpu_tex_magic(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	NodeTexMagic *tex = (NodeTexMagic*)node->storage;
	float depth = tex->depth;

	if(!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	return GPU_stack_link(mat, "node_tex_magic", in, out, GPU_uniform(&depth));
}

/* node type definition */
void register_node_type_sh_tex_magic(ListBase *lb)
{
	static bNodeType ntype;

	node_type_base(&ntype, SH_NODE_TEX_MAGIC, "Magic Texture", NODE_CLASS_TEXTURE, 0);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_magic_in, sh_node_tex_magic_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_magic);
	node_type_storage(&ntype, "NodeTexMagic", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_shader_exec_tex_magic);
	node_type_gpu(&ntype, node_shader_gpu_tex_magic);

	nodeRegisterType(lb, &ntype);
};

