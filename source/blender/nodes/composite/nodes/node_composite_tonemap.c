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

/** \file blender/nodes/composite/nodes/node_composite_tonemap.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate cmp_node_tonemap_in[]= {
	{	SOCK_RGBA, 1, N_("Image"),			1.0f, 1.0f, 1.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_tonemap_out[]= {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};


static float avgLogLum(CompBuf *src, float* auto_key, float* Lav, float* Cav)
{
	float lsum = 0;
	int p = src->x*src->y;
	fRGB* bc = (fRGB*)src->rect;
	float avl, maxl = -1e10f, minl = 1e10f;
	const float sc = 1.f/(src->x*src->y);
	*Lav = 0.f;
	while (p--) {
		float L = 0.212671f*bc[0][0] + 0.71516f*bc[0][1] + 0.072169f*bc[0][2];
		*Lav += L;
		fRGB_add(Cav, bc[0]);
		lsum += (float)log((double)MAX2(L, 0.0) + 1e-5);
		maxl = (L > maxl) ? L : maxl;
		minl = (L < minl) ? L : minl;
		bc++;
	}
	*Lav *= sc;
	fRGB_mult(Cav, sc);
	maxl = log((double)maxl + 1e-5); minl = log((double)minl + 1e-5f); avl = lsum*sc;
	*auto_key = (maxl > minl) ? ((maxl - avl) / (maxl - minl)) : 1.f;
	return exp((double)avl);
}


static void tonemap(NodeTonemap* ntm, CompBuf* dst, CompBuf* src)
{
	int x, y;
	float dr, dg, db, al, igm = (ntm->gamma==0.f) ? 1 : (1.f / ntm->gamma);
	float auto_key, Lav, Cav[3] = {0, 0, 0};

	al = avgLogLum(src, &auto_key, &Lav, Cav);
	al = (al == 0.f) ? 0.f : (ntm->key / al);

	if (ntm->type == 1) {
		// Reinhard/Devlin photoreceptor
		const float f = exp((double)-ntm->f);
		const float m = (ntm->m > 0.f) ? ntm->m : (0.3f + 0.7f*pow((double)auto_key, 1.4));
		const float ic = 1.f - ntm->c, ia = 1.f - ntm->a;
		if (ntm->m == 0.f) printf("tonemap node, M: %g\n", m); 
		for (y=0; y<src->y; ++y) {
			fRGB* sp = (fRGB*)&src->rect[y*src->x*src->type];
			fRGB* dp = (fRGB*)&dst->rect[y*src->x*src->type];
			for (x=0; x<src->x; ++x) {
				const float L = 0.212671f*sp[x][0] + 0.71516f*sp[x][1] + 0.072169f*sp[x][2];
				float I_l = sp[x][0] + ic*(L - sp[x][0]);
				float I_g = Cav[0] + ic*(Lav - Cav[0]);
				float I_a = I_l + ia*(I_g - I_l);
				dp[x][0] /= (dp[x][0] + pow((double)f*I_a, (double)m));
				I_l = sp[x][1] + ic*(L - sp[x][1]);
				I_g = Cav[1] + ic*(Lav - Cav[1]);
				I_a = I_l + ia*(I_g - I_l);
				dp[x][1] /= (dp[x][1] + pow((double)f*I_a, (double)m));
				I_l = sp[x][2] + ic*(L - sp[x][2]);
				I_g = Cav[2] + ic*(Lav - Cav[2]);
				I_a = I_l + ia*(I_g - I_l);
				dp[x][2] /= (dp[x][2] + pow((double)f*I_a, (double)m));
			}
		}
		return;
	}

	// Reinhard simple photographic tm (simplest, not using whitepoint var)
	for (y=0; y<src->y; y++) {
		fRGB* sp = (fRGB*)&src->rect[y*src->x*src->type];
		fRGB* dp = (fRGB*)&dst->rect[y*src->x*src->type];
		for (x=0; x<src->x; x++) {
			fRGB_copy(dp[x], sp[x]);
			fRGB_mult(dp[x], al);
			dr = dp[x][0] + ntm->offset;
			dg = dp[x][1] + ntm->offset;
			db = dp[x][2] + ntm->offset;
			dp[x][0] /= ((dr == 0.f) ? 1.f : dr);
			dp[x][1] /= ((dg == 0.f) ? 1.f : dg);
			dp[x][2] /= ((db == 0.f) ? 1.f : db);
			if (igm != 0.f) {
				dp[x][0] = pow((double)MAX2(dp[x][0], 0.0), igm);
				dp[x][1] = pow((double)MAX2(dp[x][1], 0.0), igm);
				dp[x][2] = pow((double)MAX2(dp[x][2], 0.0), igm);
			}
		}
	}
}


static void node_composit_exec_tonemap(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *img = in[0]->data;

	if ((img==NULL) || (out[0]->hasoutput==0)) return;

	if (img->type != CB_RGBA)
		img = typecheck_compbuf(img, CB_RGBA);
	
	new = dupalloc_compbuf(img);

	tonemap(node->storage, new, img);

	out[0]->data = new;
	
	if (img!=in[0]->data)
		free_compbuf(img);
}

static void node_composit_init_tonemap(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTonemap *ntm = MEM_callocN(sizeof(NodeTonemap), "node tonemap data");
	ntm->type = 1;
	ntm->key = 0.18;
	ntm->offset = 1;
	ntm->gamma = 1;
	ntm->f = 0;
	ntm->m = 0;	// actual value is set according to input
	// default a of 1 works well with natural HDR images, but not always so for cgi.
	// Maybe should use 0 or at least lower initial value instead
	ntm->a = 1;
	ntm->c = 0;
	node->storage = ntm;
}

void register_node_type_cmp_tonemap(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_TONEMAP, "Tonemap", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_tonemap_in, cmp_node_tonemap_out);
	node_type_size(&ntype, 150, 120, 200);
	node_type_init(&ntype, node_composit_init_tonemap);
	node_type_storage(&ntype, "NodeTonemap", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_tonemap);

	nodeRegisterType(ttype, &ntype);
}
