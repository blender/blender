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

/** \file blender/nodes/shader/nodes/node_shader_script.c
 *  \ingroup shdnodes
 */

#include "BKE_idprop.h"

#include "node_shader_util.h"

/* **************** Script ******************** */

static void init(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	NodeShaderScript *nss = MEM_callocN(sizeof(NodeShaderScript), "shader script node");
	node->storage = nss;
}

static void node_free_script(bNode *node)
{
	NodeShaderScript *nss = node->storage;

	if (nss) {
		if (nss->bytecode) {
			MEM_freeN(nss->bytecode);
		}

		if (nss->prop) {
			IDP_FreeProperty(nss->prop);
			MEM_freeN(nss->prop);
		}

		MEM_freeN(nss);
	}
}

static void node_copy_script(bNode *orig_node, bNode *new_node)
{
	NodeShaderScript *orig_nss = orig_node->storage;
	NodeShaderScript *new_nss = MEM_dupallocN(orig_nss);

	if (orig_nss->bytecode)
		new_nss->bytecode = MEM_dupallocN(orig_nss->bytecode);

	if (orig_nss->prop)
		new_nss->prop = IDP_CopyProperty(orig_nss->prop);

	new_node->storage = new_nss;
}

void register_node_type_sh_script(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_SCRIPT, "Script", NODE_CLASS_SCRIPT, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_init(&ntype, init);
	node_type_storage(&ntype, "NodeShaderScript", node_free_script, node_copy_script);

	nodeRegisterType(ttype, &ntype);
}
