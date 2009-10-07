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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../TEX_util.h"

/* **************** VALTORGB ******************** */
static bNodeSocketType valtorgb_in[]= {
	{	SOCK_VALUE, 1, "Fac",			0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType valtorgb_out[]= {
	{	SOCK_RGBA, 0, "Color",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void valtorgb_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	if(node->storage) {
		float fac = tex_input_value(in[0], p, thread);

		do_colorband(node->storage, fac, out);
	}
}

static void valtorgb_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &valtorgb_colorfn, data);
}

static void valtorgb_init(bNode *node)
{
	node->storage = add_colorband(1);
}

bNodeType tex_node_valtorgb= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_VALTORGB,
	/* name        */	"ColorRamp",
	/* width+range */	240, 200, 300,
	/* class+opts  */	NODE_CLASS_CONVERTOR, NODE_OPTIONS,
	/* input sock  */	valtorgb_in,
	/* output sock */	valtorgb_out,
	/* storage     */	"ColorBand",
	/* execfunc    */	valtorgb_exec,
	/* butfunc     */	NULL,
	/* initfunc    */	valtorgb_init,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
	
};

/* **************** RGBTOBW ******************** */
static bNodeSocketType rgbtobw_in[]= {
   {	SOCK_RGBA, 1, "Color",			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
   {	-1, 0, ""	}
};
static bNodeSocketType rgbtobw_out[]= {
   {	SOCK_VALUE, 0, "Val",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
   {	-1, 0, ""	}
};


static void rgbtobw_valuefn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float cin[4];
	tex_input_rgba(cin, in[0], p, thread);
	
	*out = cin[0] * 0.35f + cin[1] * 0.45f + cin[2] * 0.2f;
}

static void rgbtobw_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	tex_output(node, in, out[0], &rgbtobw_valuefn, data);
}

bNodeType tex_node_rgbtobw= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_RGBTOBW,
	/* name        */	"RGB to BW",
	/* width+range */	80, 40, 120,
	/* class+opts  */	NODE_CLASS_CONVERTOR, 0,
	/* input sock  */	rgbtobw_in,
	/* output sock */	rgbtobw_out,
	/* storage     */	"",
	/* execfunc    */	rgbtobw_exec,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL

};

