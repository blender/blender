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

/** \file blender/nodes/composite/nodes/node_composite_sepcombYCCA.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** SEPARATE YCCA ******************** */
static bNodeSocketTemplate cmp_node_sepycca_in[] = {
	{  SOCK_RGBA, 1, N_("Image"),        1.0f, 1.0f, 1.0f, 1.0f},
	{  -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_sepycca_out[] = {
	{  SOCK_FLOAT, 0, N_("Y")},
	{  SOCK_FLOAT, 0, N_("Cb")},
	{  SOCK_FLOAT, 0, N_("Cr")},
	{  SOCK_FLOAT, 0, N_("A")},
	{  -1, 0, ""   }
};


void register_node_type_cmp_sepycca(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_SEPYCCA, "Separate YCbCrA", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_sepycca_in, cmp_node_sepycca_out);

	nodeRegisterType(&ntype);
}



/* **************** COMBINE YCCA ******************** */
static bNodeSocketTemplate cmp_node_combycca_in[] = {
	{	SOCK_FLOAT, 1, N_("Y"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Cb"),			0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Cr"),			0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("A"),			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_combycca_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

void register_node_type_cmp_combycca(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_COMBYCCA, "Combine YCbCrA", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_combycca_in, cmp_node_combycca_out);

	nodeRegisterType(&ntype);
}
