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
* The Original Code is Copyright (C) 2006 Blender Foundation.
* All rights reserved.
* 
* The Original Code is: all of this file.
* 
* Contributor(s): none yet.
* 
* ***** END GPL LICENSE BLOCK *****

*/

#include "../CMP_util.h"

/* **************** Premul and Key Alpha Convert ******************** */

static bNodeSocketType cmp_node_premulkey_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_premulkey_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_composit_exec_premulkey(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *stackbuf, *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);

		stackbuf= dupalloc_compbuf(cbuf);
		premul_compbuf(stackbuf, node->custom1 == 1);

		out[0]->data = stackbuf;
		if(cbuf != in[0]->data)
			free_compbuf(cbuf);
	}
}

bNodeType cmp_node_premulkey= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_PREMULKEY,
	/* name        */	"Alpha Convert",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_premulkey_in,
	/* output sock */	cmp_node_premulkey_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_premulkey,
	/* butfunc     */	NULL, 
	/* initfunc    */	NULL, 
	/* freestoragefunc	*/ NULL, 
	/* copysotragefunc	*/ NULL, 
	/* id          */	NULL
};

