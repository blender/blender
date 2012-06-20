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
 * Contributor(s): Alfredo de Greef  (eeshlo)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_lensdist.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_lensdist_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, N_("Distort"), 	0.f, 0.f, 0.f, 0.f, -0.999f, 1.f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("Dispersion"), 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_lensdist_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

/* assumes *dst is type RGBA */
static void lensDistort(CompBuf *dst, CompBuf *src, float kr, float kg, float kb, int jit, int proj, int fit)
{
	int x, y, z;
	const float cx = 0.5f*(float)dst->x, cy = 0.5f*(float)dst->y;

	if (proj) {
		// shift
		CompBuf *tsrc = dupalloc_compbuf(src);
		
		for (z=0; z<tsrc->type; ++z)
			IIR_gauss(tsrc, (kr+0.5f)*(kr+0.5f), z, 1);
		kr *= 20.f;
		
		for (y=0; y<dst->y; y++) {
			fRGB *colp = (fRGB*)&dst->rect[y*dst->x*dst->type];
			const float v = (y + 0.5f)/(float)dst->y;
			
			for (x=0; x<dst->x; x++) {
				const float u = (x + 0.5f)/(float)dst->x;
				
				qd_getPixelLerpChan(tsrc, (u*dst->x + kr) - 0.5f, v*dst->y - 0.5f, 0, colp[x]);
				if (tsrc->type == CB_VAL)
					colp[x][1] = tsrc->rect[x + y*tsrc->x];
				else
					colp[x][1] = tsrc->rect[(x + y*tsrc->x)*tsrc->type + 1];
				qd_getPixelLerpChan(tsrc, (u*dst->x - kr) - 0.5f, v*dst->y - 0.5f, 2, colp[x]+2);
				
				/* set alpha */
				colp[x][3]= 1.0f;
			}
		}
		free_compbuf(tsrc);
	}
	else {
		// Spherical
		// Scale factor to make bottom/top & right/left sides fit in window after deform
		// so in the case of pincushion (kn < 0), corners will be outside window.
		// Now also optionally scales image such that black areas are not visible when distort factor is positive
		// (makes distorted corners match window corners, but really only valid if mk<=0.5)
		const float mk = MAX3(kr, kg, kb);
		const float sc = (fit && (mk > 0.f)) ? (1.f/(1.f + 2.f*mk)) : (1.f/(1.f + mk));
		const float drg = 4.f*(kg - kr), dgb = 4.f*(kb - kg);
		
		kr *= 4.f, kg *= 4.f, kb *= 4.f;

		for (y=0; y<dst->y; y++) {
			fRGB *colp = (fRGB*)&dst->rect[y*dst->x*dst->type];
			const float v = sc*((y + 0.5f) - cy)/cy;
			
			for (x=0; x<dst->x; x++) {
				int dr = 0, dg = 0, db = 0;
				float d, t, ln[6] = {0, 0, 0, 0, 0, 0};
				fRGB c1, tc = {0, 0, 0, 0};
				const float u = sc*((x + 0.5f) - cx)/cx;
				const float uv_dot = u * u + v * v;
				int sta = 0, mid = 0, end = 0;
				
				if ((t = 1.f - kr*uv_dot) >= 0.f) {
					d = 1.f/(1.f + sqrtf(t));
					ln[0] = (u*d + 0.5f)*dst->x - 0.5f, ln[1] = (v*d + 0.5f)*dst->y - 0.5f;
					sta = 1;
				}
				if ((t = 1.f - kg*uv_dot) >= 0.f) {
					d = 1.f/(1.f + sqrtf(t));
					ln[2] = (u*d + 0.5f)*dst->x - 0.5f, ln[3] = (v*d + 0.5f)*dst->y - 0.5f;
					mid = 1;
				}
				if ((t = 1.f - kb*uv_dot) >= 0.f) {
					d = 1.f/(1.f + sqrtf(t));
					ln[4] = (u*d + 0.5f)*dst->x - 0.5f, ln[5] = (v*d + 0.5f)*dst->y - 0.5f;
					end = 1;
				}
	
				if (sta && mid && end) {
					// RG
					const int dx = ln[2] - ln[0], dy = ln[3] - ln[1];
					const float dsf = sqrtf(dx*dx + dy*dy) + 1.f;
					const int ds = (int)(jit ? ((dsf < 4.f) ? 2.f : sqrtf(dsf)) : dsf);
					const float sd = 1.f/(float)ds;
					
					for (z=0; z<ds; ++z) {
						const float tz = ((float)z + (jit ? BLI_frand() : 0.5f))*sd;
						t = 1.f - (kr + tz*drg)*uv_dot;
						d = 1.f / (1.f + sqrtf(t));
						qd_getPixelLerp(src, (u*d + 0.5f)*dst->x - 0.5f, (v*d + 0.5f)*dst->y - 0.5f, c1);
						if (src->type == CB_VAL) c1[1] = c1[2] = c1[0];
						tc[0] += (1.f-tz)*c1[0], tc[1] += tz*c1[1];
						dr++, dg++;
					}
					// GB
					{
						const int dx = ln[4] - ln[2], dy = ln[5] - ln[3];
						const float dsf = sqrtf(dx*dx + dy*dy) + 1.f;
						const int ds = (int)(jit ? ((dsf < 4.f) ? 2.f : sqrtf(dsf)) : dsf);
						const float sd = 1.f/(float)ds;
						
						for (z=0; z<ds; ++z) {
							const float tz = ((float)z + (jit ? BLI_frand() : 0.5f))*sd;
							t = 1.f - (kg + tz*dgb)*uv_dot;
							d = 1.f / (1.f + sqrtf(t));
							qd_getPixelLerp(src, (u*d + 0.5f)*dst->x - 0.5f, (v*d + 0.5f)*dst->y - 0.5f, c1);
							if (src->type == CB_VAL) c1[1] = c1[2] = c1[0];
							tc[1] += (1.f-tz)*c1[1], tc[2] += tz*c1[2];
							dg++, db++;
						}
					}
				}
	
				if (dr) colp[x][0] = 2.f*tc[0] / (float)dr;
				if (dg) colp[x][1] = 2.f*tc[1] / (float)dg;
				if (db) colp[x][2] = 2.f*tc[2] / (float)db;
	
				/* set alpha */
				colp[x][3]= 1.0f;
			}
		}
	}
}


static void node_composit_exec_lensdist(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *img = in[0]->data;
	NodeLensDist *nld = node->storage;
	const float k = MAX2(MIN2(in[1]->vec[0], 1.f), -0.999f);
	// smaller dispersion range for somewhat more control
	const float d = 0.25f*MAX2(MIN2(in[2]->vec[0], 1.f), 0.f);
	const float kr = MAX2(MIN2((k+d), 1.f), -0.999f), kb = MAX2(MIN2((k-d), 1.f), -0.999f);

	if ((img==NULL) || (out[0]->hasoutput==0)) return;

	new = alloc_compbuf(img->x, img->y, CB_RGBA, 1);

	lensDistort(new, img, (nld->proj ? d : kr), k, kb, nld->jit, nld->proj, nld->fit);

	out[0]->data = new;
}


static void node_composit_init_lensdist(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeLensDist *nld = MEM_callocN(sizeof(NodeLensDist), "node lensdist data");
	nld->jit = nld->proj = nld->fit = 0;
	node->storage = nld;
}


void register_node_type_cmp_lensdist(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_LENSDIST, "Lens Distortion", NODE_CLASS_DISTORT, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_lensdist_in, cmp_node_lensdist_out);
	node_type_size(&ntype, 150, 120, 200);
	node_type_init(&ntype, node_composit_init_lensdist);
	node_type_storage(&ntype, "NodeLensDist", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_lensdist);

	nodeRegisterType(ttype, &ntype);
}
