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

#include "../CMP_util.h"


/* **************** Brigh and contrsast  ******************** */

static bNodeSocketType cmp_node_brightcontrast_in[]= {
	{	SOCK_RGBA, 1, "Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Bright",		0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f},
	{	SOCK_VALUE, 1, "Contrast",		0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 100.0f},
	{	-1, 0, ""	}
};
static bNodeSocketType cmp_node_brightcontrast_out[]= {
	{	SOCK_RGBA, 0, "Image",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void do_brightnesscontrast(bNode *node, float *out, float *in)
{
	float i;
	int c;
	float a, b, contrast, brightness, delta, v;
	contrast = node->custom2;
	brightness = (float)(node->custom1);
	brightness = (brightness) / 100.0f;
	delta = contrast / 200.0f;
	a = 1.0f - delta * 2.0f;
	/*
	* The algorithm is by Werner D. Streidt
	* (http://visca.com/ffactory/archives/5-99/msg00021.html)
	* Extracted of OpenCV demhist.c
	*/
	if( contrast > 0 )
{
		a = 1.0f / a;
		b = a * (brightness - delta);
	}
	else
	{
		delta *= -1;
		b = a * (brightness + delta);
	}
	
	for(c=0; c<3; c++){        
		i = in[c];
		v = a*i + b;
		out[c] = v;
	}
}

static void node_composit_exec_brightcontrast(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(out[0]->hasoutput==0)
		return;
	
	if(in[0]->data) {
		CompBuf *stackbuf, *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		node->custom1 = in[1]->vec[0];
		node->custom2 = in[2]->vec[0];
		stackbuf= dupalloc_compbuf(cbuf);
		composit1_pixel_processor(node, stackbuf, cbuf, in[0]->vec, do_brightnesscontrast, CB_RGBA);
		out[0]->data = stackbuf;
		if(cbuf != in[0]->data)
			free_compbuf(cbuf);
	}
}

bNodeType cmp_node_brightcontrast= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_BRIGHTCONTRAST,
	/* name        */	"Bright/Contrast",
	/* width+range */	140, 100, 320,
	/* class+opts  */	NODE_CLASS_OP_COLOR, NODE_OPTIONS,
	/* input sock  */	cmp_node_brightcontrast_in,
	/* output sock */	cmp_node_brightcontrast_out,
	/* storage     */	"",
	/* execfunc    */	node_composit_exec_brightcontrast,
	/* butfunc     */	NULL, 
	/* initfunc    */	NULL, 
	/* freestoragefunc	*/ NULL, 
	/* copysotragefunc	*/ NULL, 
	/* id          */	NULL
};

