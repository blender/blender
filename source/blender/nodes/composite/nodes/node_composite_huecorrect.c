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
 * Contributor(s): Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_huecorrect.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_huecorrect_in[] = {
	{	SOCK_FLOAT, 1, N_("Fac"),	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Image"),	1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_huecorrect_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

static void node_composit_init_huecorrect(bNodeTree *UNUSED(ntree), bNode *node)
{
	CurveMapping *cumapping = node->storage = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
	int c;
	
	cumapping->preset = CURVE_PRESET_MID9;
	
	for (c = 0; c < 3; c++) {
		CurveMap *cuma = &cumapping->cm[c];
		curvemap_reset(cuma, &cumapping->clipr, cumapping->preset, CURVEMAP_SLOPE_POSITIVE);
	}
	
	/* default to showing Saturation */
	cumapping->cur = 1;
}

void register_node_type_cmp_huecorrect(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_HUECORRECT, "Hue Correct", NODE_CLASS_OP_COLOR, 0);
	node_type_socket_templates(&ntype, cmp_node_huecorrect_in, cmp_node_huecorrect_out);
	node_type_size(&ntype, 320, 140, 500);
	node_type_init(&ntype, node_composit_init_huecorrect);
	node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);

	nodeRegisterType(&ntype);
}
