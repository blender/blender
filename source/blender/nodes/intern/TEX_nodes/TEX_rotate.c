/**
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>

#include "../TEX_util.h"

static bNodeSocketType inputs[]= { 
	{ SOCK_RGBA, 1, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ SOCK_VALUE, 1, "Turns",   0.0f, 0.0f, 0.0f, 0.0f,  -1.0f, 1.0f },
	{ SOCK_VECTOR, 1, "Axis",   0.0f, 0.0f, 1.0f, 0.0f,  -1.0f, 1.0f },
	{ -1, 0, "" } 
};

static bNodeSocketType outputs[]= { 
	{ SOCK_RGBA, 0, "Color", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static void rotate(float new_co[3], float a, float ax[3], float co[3])
{
	float para[3];
	float perp[3];
	float cp[3];
	
	float cos_a = cos(a * 2 * M_PI);
	float sin_a = sin(a * 2 * M_PI);
	
	// x' = xcosa + n(n.x)(1-cosa) + (x*n)sina
	
	mul_v3_v3fl(perp, co, cos_a);
	mul_v3_v3fl(para, ax, dot_v3v3(co, ax)*(1 - cos_a));
	
	cross_v3_v3v3(cp, ax, co);
	mul_v3_fl(cp, sin_a);
	
	new_co[0] = para[0] + perp[0] + cp[0];
	new_co[1] = para[1] + perp[1] + cp[1];
	new_co[2] = para[2] + perp[2] + cp[2];
}

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float new_co[3], new_dxt[3], new_dyt[3], a, ax[3];
	
	a= tex_input_value(in[1], p, thread);
	tex_input_vec(ax, in[2], p, thread);

	rotate(new_co, a, ax, p->co);
	if (p->osatex) {
		rotate(new_dxt, a, ax, p->dxt);
		rotate(new_dyt, a, ax, p->dyt);
	}
	
	{
		TexParams np = *p;
		np.co = new_co;
		np.dxt = new_dxt;
		np.dyt = new_dyt;
		tex_input_rgba(out, in[0], &np, thread);
	}
}
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out) 
{
	tex_output(node, in, out[0], &colorfn, data);
}

bNodeType tex_node_rotate= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	TEX_NODE_ROTATE, 
	/* name        */	"Rotate", 
	/* width+range */	90, 80, 100, 
	/* class+opts  */	NODE_CLASS_DISTORT, NODE_OPTIONS, 
	/* input sock  */	inputs, 
	/* output sock */	outputs, 
	/* storage     */	"", 
	/* execfunc    */	exec,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
};

