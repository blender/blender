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
 * Contributor(s): Vilem Novak
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_bilateralblur.c
 *  \ingroup cmpnodes
 */

#include "node_composite_util.h"

/* **************** BILATERALBLUR ******************** */
static bNodeSocketTemplate cmp_node_bilateralblur_in[]= {
	{ SOCK_RGBA, 1, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f}, 
	{ SOCK_RGBA, 1, N_("Determinator"), 1.0f, 1.0f, 1.0f, 1.0f}, 
	{ -1, 0, "" } 
};

static bNodeSocketTemplate cmp_node_bilateralblur_out[]= { 
	{ SOCK_RGBA, 0, N_("Image")}, 
	{ -1, 0, "" } 
};

#define INIT_C3                                                               \
	mean0 = 1;                                                                \
	mean1[0] = src[0];                                                        \
	mean1[1] = src[1];                                                        \
	mean1[2] = src[2];                                                        \
	mean1[3] = src[3];                                                        \
	(void)0

/* finds color distances */
#define COLOR_DISTANCE_C3(c1, c2)                                             \
	((c1[0] - c2[0])*(c1[0] - c2[0]) +                                        \
	 (c1[1] - c2[1])*(c1[1] - c2[1]) +                                        \
	 (c1[2] - c2[2])*(c1[2] - c2[2]) +                                        \
	 (c1[3] - c2[3])*(c1[3] - c2[3]))

/* this is the main kernel function for comparing color distances
 and adding them weighted to the final color */
#define KERNEL_ELEMENT_C3(k)                                                  \
	temp_color = src + deltas[k];                                             \
	ref_color = ref + deltas[k];                                              \
	w = weight_tab[k] +                                                       \
		(double)COLOR_DISTANCE_C3(ref, ref_color ) * i2sigma_color;           \
	w = 1.0/(w*w + 1);                                                        \
	mean0 += w;                                                               \
	mean1[0] += (double)temp_color[0] * w;                                    \
	mean1[1] += (double)temp_color[1] * w;                                    \
	mean1[2] += (double)temp_color[2] * w;                                    \
	mean1[3] += (double)temp_color[3] * w;                                    \
	(void)0

/* write blurred values to image */
#define UPDATE_OUTPUT_C3                                                      \
	mean0 = 1.0/mean0;                                                        \
	dest[x * pix + 0] = mean1[0] * mean0;                                     \
	dest[x * pix + 1] = mean1[1] * mean0;                                     \
	dest[x * pix + 2] = mean1[2] * mean0;                                     \
	dest[x * pix + 3] = mean1[3] * mean0;                                     \
	(void)0

/* initializes deltas for fast access to neighbor pixels */
#define INIT_3X3_DELTAS( deltas, step, nch )                                  \
	((deltas)[0] =  (nch),  (deltas)[1] = -(step) + (nch),                    \
	 (deltas)[2] = -(step), (deltas)[3] = -(step) - (nch),                    \
	 (deltas)[4] = -(nch),  (deltas)[5] =  (step) - (nch),                    \
	 (deltas)[6] =  (step), (deltas)[7] =  (step) + (nch));                   \
	(void)0


/* code of this node was heavily inspired by the smooth function of opencv library.
 * The main change is an optional image input */
static void node_composit_exec_bilateralblur(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	NodeBilateralBlurData *nbbd= node->storage;
	CompBuf *new, *source, *img= in[0]->data, *refimg= in[1]->data;
	double mean0, w, i2sigma_color, i2sigma_space;
	double mean1[4];
	double weight_tab[8];
	float *src, *dest, *ref, *temp_color, *ref_color;
	float sigma_color, sigma_space;
	int imgx, imgy, x, y, pix, i, step;
	int deltas[8];
	short found_determinator= 0;

	if (img == NULL || out[0]->hasoutput == 0)
		return;

	if (img->type != CB_RGBA) {
		img= typecheck_compbuf(in[0]->data, CB_RGBA);
	}

	imgx= img->x;
	imgy= img->y;
	pix= img->type;
	step= pix * imgx;

	if (refimg) {
		if (refimg->x == imgx && refimg->y == imgy) {
			if (ELEM3(refimg->type, CB_VAL, CB_VEC2, CB_VEC3)) {
				refimg= typecheck_compbuf(in[1]->data, CB_RGBA);
				found_determinator= 1;
			}
		}
	}
	else {
		refimg= img;
	}

	/* allocs */
	source= dupalloc_compbuf(img);
	new= alloc_compbuf(imgx, imgy, pix, 1);
	
	/* accept image offsets from other nodes */
	new->xof= img->xof;
	new->yof= img->yof;

	/* bilateral code properties */
	sigma_color= nbbd->sigma_color;
	sigma_space= nbbd->sigma_space;
	
	i2sigma_color= 1.0f / (sigma_color * sigma_color);
	i2sigma_space= 1.0f / (sigma_space * sigma_space);

	INIT_3X3_DELTAS(deltas, step, pix);

	weight_tab[0] = weight_tab[2] = weight_tab[4] = weight_tab[6] = i2sigma_space;
	weight_tab[1] = weight_tab[3] = weight_tab[5] = weight_tab[7] = i2sigma_space * 2;

	/* iterations */
	for (i= 0; i < nbbd->iter; i++) {
		src= source->rect;
		ref= refimg->rect;
		dest= new->rect;
		/*goes through image, there are more loops for 1st/last line and all other lines*/
		/*kernel element accumulates surrounding colors, which are then written with the update_output function*/
		for (x= 0; x < imgx; x++, src+= pix, ref+= pix) {
			INIT_C3;

			KERNEL_ELEMENT_C3(6);

			if (x > 0) {
				KERNEL_ELEMENT_C3(5);
				KERNEL_ELEMENT_C3(4);
			}

			if (x < imgx - 1) {
				KERNEL_ELEMENT_C3(7);
				KERNEL_ELEMENT_C3(0);
			}

			UPDATE_OUTPUT_C3;
		}

		dest+= step;

		for (y= 1; y < imgy - 1; y++, dest+= step, src+= pix, ref+= pix) {
			x= 0;

			INIT_C3;

			KERNEL_ELEMENT_C3(0);
			KERNEL_ELEMENT_C3(1);
			KERNEL_ELEMENT_C3(2);
			KERNEL_ELEMENT_C3(6);
			KERNEL_ELEMENT_C3(7);

			UPDATE_OUTPUT_C3;

			src+= pix;
			ref+= pix;

			for (x= 1; x < imgx - 1; x++, src+= pix, ref+= pix) {
				INIT_C3;

				KERNEL_ELEMENT_C3(0);
				KERNEL_ELEMENT_C3(1);
				KERNEL_ELEMENT_C3(2);
				KERNEL_ELEMENT_C3(3);
				KERNEL_ELEMENT_C3(4);
				KERNEL_ELEMENT_C3(5);
				KERNEL_ELEMENT_C3(6);
				KERNEL_ELEMENT_C3(7);

				UPDATE_OUTPUT_C3;
			}

			INIT_C3;

			KERNEL_ELEMENT_C3(2);
			KERNEL_ELEMENT_C3(3);
			KERNEL_ELEMENT_C3(4);
			KERNEL_ELEMENT_C3(5);
			KERNEL_ELEMENT_C3(6);

			UPDATE_OUTPUT_C3;
		}

		for (x= 0; x < imgx; x++, src+= pix, ref+= pix) {
			INIT_C3;

			KERNEL_ELEMENT_C3(2);

			if (x > 0) {
				KERNEL_ELEMENT_C3(3);
				KERNEL_ELEMENT_C3(4);
			}
			if (x < imgx - 1) {
				KERNEL_ELEMENT_C3(1);
				KERNEL_ELEMENT_C3(0);
			}

			UPDATE_OUTPUT_C3;
		}

		if (node->exec & NODE_BREAK) break;

		SWAP(CompBuf, *source, *new);
	}

	if (img != in[0]->data)
		free_compbuf(img);

	if (found_determinator == 1) {
		if (refimg != in[1]->data)
			free_compbuf(refimg);
	}

	out[0]->data= source;

	free_compbuf(new);
}

static void node_composit_init_bilateralblur(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeBilateralBlurData *nbbd= MEM_callocN(sizeof(NodeBilateralBlurData), "node bilateral blur data");
	node->storage= nbbd;
	nbbd->sigma_color= 0.3;
	nbbd->sigma_space= 5.0;
}

void register_node_type_cmp_bilateralblur(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_BILATERALBLUR, "Bilateral Blur", NODE_CLASS_OP_FILTER, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_bilateralblur_in, cmp_node_bilateralblur_out);
	node_type_size(&ntype, 150, 120, 200);
	node_type_init(&ntype, node_composit_init_bilateralblur);
	node_type_storage(&ntype, "NodeBilateralBlurData", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_bilateralblur);

	nodeRegisterType(ttype, &ntype);
}
