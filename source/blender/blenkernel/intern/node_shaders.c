/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <stdlib.h>

#include "DNA_ID.h"
#include "DNA_node_types.h"

#include "BKE_blender.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

/* **************** testnode ************ */

static void blendcolor(float *col1, float *col2, float *output, float fac)
{
	output[0]= (1.0f-fac)*col1[0] + (fac)*col2[0];
	output[1]= (1.0f-fac)*col1[1] + (fac)*col2[1];
	output[2]= (1.0f-fac)*col1[2] + (fac)*col2[2];
}

static void node_shader_exec_test(bNode *node, bNodeStack **ns)
{
	
	blendcolor(ns[0]->vec, ns[1]->vec, ns[2]->vec, 0.5);

//	printvecf(node->name, ns[2]->vec);
}

static bNode *node_shader_add_test(bNodeTree *ntree)
{
	bNode *node= nodeAddNode(ntree, "TestNode");
	static int tot= 0;
	
	sprintf(node->name, "Testnode%d", tot++);
	node->type= SH_NODE_TEST;
	node->width= 80.0f;
	
	/* add sockets */
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "Col");
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "Spec");
	nodeAddSocket(node, SOCK_RGBA, SOCK_OUT, 0xFFF, "Diffuse");
	
	return node;
}

/* **************** value node ************ */

static void node_shader_exec_value(bNode *node, bNodeStack **ns)
{
	/* no input node! */
	ns[0]->vec[0]= node->ns.vec[0];
//	printf("%s %f\n", node->name, ns[0]->vec[0]);
}

static bNode *node_shader_add_value(bNodeTree *ntree)
{
	bNode *node= nodeAddNode(ntree, "Value");
	
	node->type= SH_NODE_VALUE;
	node->width= 80.0f;
	node->prv_h= 20.0f;
	
	/* add sockets */
	nodeAddSocket(node, SOCK_VALUE, SOCK_OUT, 0xFFF, "");
	
	return node;
}

/* **************** rgba node ************ */

static void node_shader_exec_rgb(bNode *node, bNodeStack **ns)
{
	/* no input node! */
	QUATCOPY(ns[0]->vec, node->ns.vec);
	
//	printvecf(node->name, ns[0]->vec);
}

static bNode *node_shader_add_rgb(bNodeTree *ntree)
{
	bNode *node= nodeAddNode(ntree, "RGB");
	
	node->type= SH_NODE_RGB;
	node->width= 100.0f;
	node->prv_h= 100.0f;
	node->ns.vec[3]= 1.0f;		/* alpha init */
	
	/* add sockets */
	nodeAddSocket(node, SOCK_RGBA, SOCK_OUT, 0xFFF, "");
	
	return node;
}

/* **************** mix rgba node ************ */

static void node_shader_exec_mix_rgb(bNode *node, bNodeStack **ns)
{
	/* stack order is fac, col1, col2, out */
	blendcolor(ns[1]->vec, ns[2]->vec, ns[3]->vec, ns[0]->vec[0]);
}

static bNode *node_shader_add_mix_rgb(bNodeTree *ntree)
{
	bNode *node= nodeAddNode(ntree, "Mix RGB");
	
	node->type= SH_NODE_MIX_RGB;
	node->width= 80.0f;
	node->prv_h= 0.0f;
	
	/* add sockets */
	nodeAddSocket(node, SOCK_VALUE, SOCK_IN, 1, "Fac");
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "Color1");
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "Color2");
	nodeAddSocket(node, SOCK_RGBA, SOCK_OUT, 0xFFF, "Color");
	
	return node;
}


/* **************** show rgba node ************ */

static void node_shader_exec_show_rgb(bNode *node, bNodeStack **ns)
{
	/* only input node! */
	QUATCOPY(node->ns.vec, ns[0]->vec);
	
//	printvecf(node->name, ns[0]->vec);
}

static bNode *node_shader_add_show_rgb(bNodeTree *ntree)
{
	bNode *node= nodeAddNode(ntree, "Show RGB");
	
	node->type= SH_NODE_SHOW_RGB;
	node->width= 80.0f;
	node->prv_h= 0.0f;
	node->ns.vec[3]= 1.0f;		/* alpha init */
	
	/* add sockets */
	nodeAddSocket(node, SOCK_RGBA, SOCK_IN, 1, "");
	
	return node;
}


/* **************** API for add ************** */

bNode *node_shader_add(bNodeTree *ntree, int type)
{
	bNode *node= NULL;
	
	switch(type) {
	case SH_NODE_TEST:
		node= node_shader_add_test(ntree);
		break;
	case SH_NODE_VALUE:
		node= node_shader_add_value(ntree);
		break;
	case SH_NODE_RGB:
		node= node_shader_add_rgb(ntree);
		break;
	case SH_NODE_SHOW_RGB:
		node= node_shader_add_show_rgb(ntree);
		break;
	case SH_NODE_MIX_RGB:
		node= node_shader_add_mix_rgb(ntree);
		break;
	}
	return node;
}

/* ******************* set the callbacks, called from UI, loader ***** */

void node_shader_set_execfunc(bNode *node)
{
	switch(node->type) {
	case SH_NODE_TEST:
		node->execfunc= node_shader_exec_test;
		break;
	case SH_NODE_VALUE:
		node->execfunc= node_shader_exec_value;
		break;
	case SH_NODE_RGB:
		node->execfunc= node_shader_exec_rgb;
		break;
	case SH_NODE_SHOW_RGB:
		node->execfunc= node_shader_exec_show_rgb;
		break;
	case SH_NODE_MIX_RGB:
		node->execfunc= node_shader_exec_mix_rgb;
		break;
	}
}

