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

/** \file blender/nodes/composite/nodes/node_composite_brightness.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** Brigh and contrsast  ******************** */

static bNodeSocketTemplate cmp_node_brightcontrast_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Bright",		0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, "Contrast",		0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_brightcontrast_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

static void do_brightnesscontrast(bNode *UNUSED(node), float *out, float *in, float *in_brightness, float *in_contrast)
{
	float i;
	int c;
	float a, b, v;
	float brightness = (*in_brightness) / 100.0f;
	float contrast = *in_contrast;
	float delta = contrast / 200.0f;
	a = 1.0f - delta * 2.0f;
	/*
	* The algorithm is by Werner D. Streidt
	* (http://visca.com/ffactory/archives/5-99/msg00021.html)
	* Extracted of OpenCV demhist.c
	*/
	if (contrast > 0) {
		a = 1.0f / a;
		b = a * (brightness - delta);
	}
	else {
		delta *= -1;
		b = a * (brightness + delta);
	}
	
	for (c=0; c<3; c++) {        
		i = in[c];
		v = a*i + b;
		out[c] = v;
	}
}

static void node_composit_exec_brightcontrast(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (out[0]->hasoutput==0)
		return;
	
	if (in[0]->data) {
		CompBuf *stackbuf, *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		stackbuf= dupalloc_compbuf(cbuf);
		composit3_pixel_processor(node, stackbuf, in[0]->data, in[0]->vec, in[1]->data, in[1]->vec, in[2]->data, in[2]->vec, do_brightnesscontrast, CB_RGBA, CB_VAL, CB_VAL);
		out[0]->data = stackbuf;
		if (cbuf != in[0]->data)
			free_compbuf(cbuf);
	}
}

void register_node_type_cmp_brightcontrast(bNodeTreeType *ttype)
{
	static bNodeType ntype;
	
	node_type_base(ttype, &ntype, CMP_NODE_BRIGHTCONTRAST, "Bright/Contrast", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_brightcontrast_in, cmp_node_brightcontrast_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_brightcontrast);
	
	nodeRegisterType(ttype, &ntype);
}
