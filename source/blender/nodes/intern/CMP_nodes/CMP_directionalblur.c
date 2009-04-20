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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Alfredo de Greef  (eeshlo)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../CMP_util.h"

static bNodeSocketType cmp_node_dblur_in[]= {
	{	SOCK_RGBA, 1, "Image", 0.8f, 0.8f, 0.8f, 1.f, 0.f, 1.f},
	{	-1, 0, ""       }
};

static bNodeSocketType cmp_node_dblur_out[]= {
	{	SOCK_RGBA, 0, "Image", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""       }
};

static CompBuf *dblur(bNode *node, CompBuf *img, int iterations, int wrap,
		  float center_x, float center_y, float dist, float angle, float spin, float zoom)
{
	if ((dist != 0.f) || (spin != 0.f) || (zoom != 0.f)) {
		void (*getpix)(CompBuf*, float, float, float*) = wrap ? qd_getPixelLerpWrap : qd_getPixelLerp;
		const float a= angle * (float)M_PI / 180.f;
		const float itsc= 1.f / pow(2.f, (float)iterations);
		float D;
		float center_x_pix, center_y_pix;
		float tx, ty;
		float sc, rot;
		CompBuf *tmp;
		int i, j;
		
		tmp= dupalloc_compbuf(img);
		
		D= dist * sqrtf(img->x * img->x + img->y * img->y);
		center_x_pix= center_x * img->x;
		center_y_pix= center_y * img->y;

		tx=  itsc * D * cos(a);
		ty= -itsc * D * sin(a);
		sc=  itsc * zoom;
		rot= itsc * spin * (float)M_PI / 180.f;

		/* blur the image */
		for(i= 0; i < iterations; ++i) {
			const float cs= cos(rot), ss= sin(rot);
			const float isc= 1.f / (1.f + sc);
			unsigned int x, y;
			float col[4]= {0,0,0,0};

			for(y= 0; y < img->y; ++y) {
				const float v= isc * (y - center_y_pix) + ty;

				for(x= 0; x < img->x; ++x) {
					const float  u= isc * (x - center_x_pix) + tx;
					unsigned int p= (x + y * img->x) * img->type;

					getpix(tmp, cs * u + ss * v + center_x_pix, cs * v - ss * u + center_y_pix, col);

					/* mix img and transformed tmp */
					for(j= 0; j < 4; ++j)
						img->rect[p + j]= AVG2(img->rect[p + j], col[j]);
				}
			}

			/* copy img to tmp */
			if(i != (iterations - 1)) 
				memcpy(tmp->rect, img->rect, sizeof(float) * img->x * img->y * img->type);

			/* double transformations */
			tx *= 2.f, ty  *= 2.f;
			sc *= 2.f, rot *= 2.f;

			if(node->exec & NODE_BREAK) break;
		}

		free_compbuf(tmp);
	}

	return img;
}

static void node_composit_exec_dblur(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	NodeDBlurData *ndbd= node->storage;
	CompBuf *new, *img= in[0]->data;
	
	if((img == NULL) || (out[0]->hasoutput == 0)) return;

	if (img->type != CB_RGBA)
		new = typecheck_compbuf(img, CB_RGBA);
	else
		new = dupalloc_compbuf(img);
	
	out[0]->data= dblur(node, new, ndbd->iter, ndbd->wrap, ndbd->center_x, ndbd->center_y, ndbd->distance, ndbd->angle, ndbd->spin, ndbd->zoom);
}

static void node_composit_init_dblur(bNode* node)
{
	NodeDBlurData *ndbd= MEM_callocN(sizeof(NodeDBlurData), "node dblur data");
	node->storage= ndbd;
	ndbd->center_x= 0.5;
	ndbd->center_y= 0.5;
}

bNodeType cmp_node_dblur = {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	CMP_NODE_DBLUR,
	/* name        */	"Directional Blur",
	/* width+range */	150, 120, 200,
	/* class+opts  */	NODE_CLASS_OP_FILTER, NODE_OPTIONS,
	/* input sock  */	cmp_node_dblur_in,
	/* output sock */	cmp_node_dblur_out,
	/* storage     */	"NodeDBlurData",
	/* execfunc    */	node_composit_exec_dblur,
	/* butfunc     */	NULL,
	/* initfunc    */	node_composit_init_dblur,
	/* freestoragefunc    */	node_free_standard_storage,
	/* copystoragefunc    */	node_copy_standard_storage,
	/* id          */	NULL
};
