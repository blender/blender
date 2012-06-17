/*
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

/** \file blender/nodes/composite/nodes/node_composite_bokehimage.c
 *  \ingroup cmpnodes
 */


#include "../node_composite_util.h"

/* **************** Bokeh image Tools  ******************** */
  
static bNodeSocketTemplate cmp_node_bokehimage_out[]= {
	{	SOCK_RGBA, 0, N_("Image"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static void node_composit_init_bokehimage(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeBokehImage * data = MEM_callocN(sizeof(NodeBokehImage), "NodeBokehImage");
	data->angle = 0.0f;
	data->flaps = 5;
	data->rounding = 0.0f;
	data->catadioptric = 0.0f;
	data->lensshift = 0.0f;
	node->storage = data;
}

void register_node_type_cmp_bokehimage(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, CMP_NODE_BOKEHIMAGE, "Bokeh Image", NODE_CLASS_INPUT, NODE_PREVIEW|NODE_OPTIONS);
	node_type_socket_templates(&ntype, NULL, cmp_node_bokehimage_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_init(&ntype, node_composit_init_bokehimage);
	node_type_storage(&ntype, "NodeBokehImage", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(ttype, &ntype);
}
