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
 * Contributor(s): Jucas.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_valToNor.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

static bNodeSocketTemplate inputs[] = {
	{ SOCK_FLOAT, 1, N_("Val"),   0.0f,   0.0f, 0.0f, 1.0f,  0.0f,   1.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("Nabla"), 0.025f, 0.0f, 0.0f, 0.0f,  0.001f, 0.1f, PROP_UNSIGNED },
	{ -1, 0, "" }
};

static bNodeSocketTemplate outputs[] = {
	{ SOCK_VECTOR, 0, N_("Normal") },
	{ -1, 0, "" }
};

static void normalfn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
	float new_co[3];
	const float *co = p->co;

	float nabla = tex_input_value(in[1], p, thread);
	float val;
	float nor[3];
	
	TexParams np = *p;
	np.co = new_co;

	val = tex_input_value(in[0], p, thread);

	new_co[0] = co[0] + nabla;
	new_co[1] = co[1];
	new_co[2] = co[2];
	nor[0] = tex_input_value(in[0], &np, thread);

	new_co[0] = co[0];
	new_co[1] = co[1] + nabla;
	nor[1] = tex_input_value(in[0], &np, thread);
	
	new_co[1] = co[1];
	new_co[2] = co[2] + nabla;
	nor[2] = tex_input_value(in[0], &np, thread);

	out[0] = val - nor[0];
	out[1] = val - nor[1];
	out[2] = val - nor[2];
}
static void exec(void *data, int UNUSED(thread), bNode *node, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, execdata, in, out[0], &normalfn, data);
}

void register_node_type_tex_valtonor(void)
{
	static bNodeType ntype;
	
	tex_node_type_base(&ntype, TEX_NODE_VALTONOR, "Value to Normal", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, inputs, outputs);
	node_type_exec(&ntype, NULL, NULL, exec);
	
	nodeRegisterType(&ntype);
}
