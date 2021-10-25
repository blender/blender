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
 * Contributor(s): Björn C. Schaefer
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_curves.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** CURVE Time  ******************** */

/* custom1 = sfra, custom2 = efra */
static bNodeSocketTemplate cmp_node_time_out[] = {
	{	SOCK_FLOAT, 0, N_("Fac")},
	{	-1, 0, ""	}
};

static void node_composit_init_curves_time(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->custom1 = 1;
	node->custom2 = 250;
	node->storage = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_time(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_TIME, "Time", NODE_CLASS_INPUT, 0);
	node_type_socket_templates(&ntype, NULL, cmp_node_time_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_curves_time);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

	nodeRegisterType(&ntype);
}



/* **************** CURVE VEC  ******************** */
static bNodeSocketTemplate cmp_node_curve_vec_in[] = {
	{	SOCK_VECTOR, 1, N_("Vector"),	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_curve_vec_out[] = {
	{	SOCK_VECTOR, 0, N_("Vector")},
	{	-1, 0, ""	}
};

static void node_composit_init_curve_vec(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->storage = curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_vec(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_curve_vec_in, cmp_node_curve_vec_out);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, node_composit_init_curve_vec);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

	nodeRegisterType(&ntype);
}


/* **************** CURVE RGB  ******************** */
static bNodeSocketTemplate cmp_node_curve_rgb_in[] = {
	{	SOCK_FLOAT, 1, N_("Fac"),	1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Image"),	1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_RGBA, 1, N_("Black Level"),	0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, N_("White Level"),	1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_curve_rgb_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void node_composit_init_curve_rgb(bNodeTree *UNUSED(ntree), bNode *node)
{
	node->storage = curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_cmp_curve_rgb(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, cmp_node_curve_rgb_in, cmp_node_curve_rgb_out);
	node_type_size(&ntype, 200, 140, 320);
	node_type_init(&ntype, node_composit_init_curve_rgb);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

	nodeRegisterType(&ntype);
}
