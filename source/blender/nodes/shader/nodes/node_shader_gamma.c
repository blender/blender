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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 * 
 * The Original Code is: all of this file.
 * 
 * Contributor(s): none yet.
 * 
 * ***** END GPL LICENSE BLOCK *****

*/

#include "node_shader_util.h"

/* **************** Gamma Tools  ******************** */
  
static bNodeSocketTemplate sh_node_gamma_in[]= {
	{	SOCK_RGBA, 1, "Color",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Gamma",			1.0f, 0.0f, 0.0f, 0.0f, 0.001f, 10.0f, PROP_UNSIGNED},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_gamma_out[]= {
	{	SOCK_RGBA, 0, "Color"},
	{	-1, 0, ""	}
};

void register_node_type_sh_gamma(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, SH_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_gamma_in, sh_node_gamma_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, NULL);
	node_type_storage(&ntype, "", NULL, NULL);
	node_type_exec(&ntype, NULL);
	node_type_gpu(&ntype, NULL);
	
	nodeRegisterType(ttype, &ntype);
}
