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

/** \file blender/nodes/composite/nodes/node_composite_gamma.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* **************** Gamma Tools  ******************** */

static bNodeSocketTemplate cmp_node_gamma_in[] = {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Gamma"),			1.0f, 0.0f, 0.0f, 0.0f, 0.001f, 10.0f, PROP_UNSIGNED},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_gamma_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

void register_node_type_cmp_gamma(void)
{
	static bNodeType ntype;
	
	cmp_node_type_base(&ntype, CMP_NODE_GAMMA, "Gamma", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, cmp_node_gamma_in, cmp_node_gamma_out);
	
	nodeRegisterType(&ntype);
}
