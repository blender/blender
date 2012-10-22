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

/** \file blender/nodes/composite/nodes/node_composite_despeckle.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** FILTER  ******************** */
static bNodeSocketTemplate cmp_node_despeckle_in[] = {
	{	SOCK_FLOAT, 1, N_("Fac"),			1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_despeckle_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

#ifdef WITH_COMPOSITOR_LEGACY

static void node_composit_exec_despeckle(void *UNUSED(data), bNode *UNUSED(node), bNodeStack **UNUSED(in), bNodeStack **UNUSED(out))
{
	/* pass */
}

#endif  /* WITH_COMPOSITOR_LEGACY */

static void node_composit_init_despeckle(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	node->custom3 = 0.5f;
	node->custom4 = 0.5f;
}

void register_node_type_cmp_despeckle(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_DESPECKLE, "Despeckle", NODE_CLASS_OP_FILTER, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_despeckle_in, cmp_node_despeckle_out);
	node_type_size(&ntype, 80, 40, 120);
	node_type_init(&ntype, node_composit_init_despeckle);
#ifdef WITH_COMPOSITOR_LEGACY
	node_type_exec(&ntype, node_composit_exec_despeckle);
#endif

	nodeRegisterType(ttype, &ntype);
}
