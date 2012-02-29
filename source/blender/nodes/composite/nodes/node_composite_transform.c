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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_transform.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** Transform  ******************** */

static bNodeSocketTemplate cmp_node_transform_in[]= {
	{	SOCK_RGBA,		1,	"Image",			0.8f, 0.8f, 0.8f, 1.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT,		1,	"X",				0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	SOCK_FLOAT,		1,	"Y",				0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
	{	SOCK_FLOAT,		1,	"Angle",			0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_ANGLE},
	{	SOCK_FLOAT,		1,	"Scale",			1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, CMP_SCALE_MAX},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate cmp_node_transform_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};

CompBuf* node_composit_transform(CompBuf *cbuf, float x, float y, float angle, float scale, int filter_type)
{
	CompBuf *stackbuf= alloc_compbuf(cbuf->x, cbuf->y, CB_RGBA, 1);
	ImBuf *ibuf, *obuf;
	float mat[4][4], lmat[4][4], rmat[4][4], smat[4][4], cmat[4][4], icmat[4][4];
	float svec[3]= {scale, scale, scale}, loc[2]= {x, y};

	unit_m4(rmat);
	unit_m4(lmat);
	unit_m4(smat);
	unit_m4(cmat);

	/* image center as rotation center */
	cmat[3][0]= (float)cbuf->x/2.0f;
	cmat[3][1]= (float)cbuf->y/2.0f;
	invert_m4_m4(icmat, cmat);

	size_to_mat4(smat, svec);		/* scale matrix */
	add_v2_v2(lmat[3], loc);		/* tranlation matrix */
	rotate_m4(rmat, 'Z', angle);	/* rotation matrix */

	/* compose transformation matrix */
	mul_serie_m4(mat, lmat, cmat, rmat, smat, icmat, NULL, NULL, NULL);

	invert_m4(mat);

	ibuf= IMB_allocImBuf(cbuf->x, cbuf->y, 32, 0);
	obuf= IMB_allocImBuf(stackbuf->x, stackbuf->y, 32, 0);

	if(ibuf && obuf) {
		int i, j;

		ibuf->rect_float= cbuf->rect;
		obuf->rect_float= stackbuf->rect;

		for(j=0; j<cbuf->y; j++) {
			for(i=0; i<cbuf->x;i++) {
				float vec[3]= {i, j, 0};

				mul_v3_m4v3(vec, mat, vec);

				switch(filter_type) {
					case 0:
						neareast_interpolation(ibuf, obuf, vec[0], vec[1], i, j);
						break;
					case 1:
						bilinear_interpolation(ibuf, obuf, vec[0], vec[1], i, j);
						break;
					case 2:
						bicubic_interpolation(ibuf, obuf, vec[0], vec[1], i, j);
						break;
				}
			}
		}

		IMB_freeImBuf(ibuf);
		IMB_freeImBuf(obuf);
	}

	/* pass on output and free */
	return stackbuf;
}

static void node_composit_exec_transform(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(in[0]->data) {
		CompBuf *cbuf= typecheck_compbuf(in[0]->data, CB_RGBA);
		CompBuf *stackbuf;

		stackbuf= node_composit_transform(cbuf, in[1]->vec[0], in[2]->vec[0], in[3]->vec[0], in[4]->vec[0], node->custom1);

		/* pass on output and free */
		out[0]->data= stackbuf;

		if(cbuf!=in[0]->data)
			free_compbuf(cbuf);
	}
}

void register_node_type_cmp_transform(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_TRANSFORM, "Transform", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_transform_in, cmp_node_transform_out);
	node_type_size(&ntype, 140, 100, 320);
	node_type_exec(&ntype, node_composit_exec_transform);

	nodeRegisterType(ttype, &ntype);
}
