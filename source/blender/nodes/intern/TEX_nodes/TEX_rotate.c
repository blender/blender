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

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
	float new_coord[3];
	float *coord = p->coord;
	
	float ax[4];
	float para[3];
	float perp[3];
	float cp[3];
	
	float magsq, ndx;
	
	float a = tex_input_value(in[1], p, thread);
	float cos_a = cos(a * 2 * M_PI);
	float sin_a = sin(a * 2 * M_PI);
	
	// x' = xcosa + n(n.x)(1-cosa)+(x*n)sina
	
	tex_input_vec(ax, in[2], p, thread);
	magsq = ax[0]*ax[0] + ax[1]*ax[1] + ax[2]*ax[2];
	
	if(magsq == 0) magsq = 1;
	
	ndx = Inpf(coord, ax);
	
	para[0] = ax[0] * ndx * (1 - cos_a);
	para[1] = ax[1] * ndx * (1 - cos_a);
	para[2] = ax[2] * ndx * (1 - cos_a);
	
	VecSubf(perp, coord, para);
	
	perp[0] = coord[0] * cos_a;
	perp[1] = coord[1] * cos_a;
	perp[2] = coord[2] * cos_a;
	
	Crossf(cp, ax, coord);
	
	cp[0] = cp[0] * sin_a;
	cp[1] = cp[1] * sin_a;
	cp[2] = cp[2] * sin_a;
	
	new_coord[0] = para[0] + perp[0] + cp[0];
	new_coord[1] = para[1] + perp[1] + cp[1];
	new_coord[2] = para[2] + perp[2] + cp[2];
	
	{
		TexParams np = *p;
		np.coord = new_coord;
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

