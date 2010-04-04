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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../SHD_util.h"

/* **************** MATERIAL ******************** */

static bNodeSocketType sh_node_material_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Spec",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Refl",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_material_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	-1, 0, ""	}
};

/* **************** EXTENDED MATERIAL ******************** */

static bNodeSocketType sh_node_material_ext_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Spec",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Refl",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA, 1, "Mirror",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Ambient",	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Emit",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "SpecTra",	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Ray Mirror",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Alpha",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 1, "Translucency",	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static bNodeSocketType sh_node_material_ext_out[]= {
	{	SOCK_RGBA, 0, "Color",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_VALUE, 0, "Alpha",		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_VECTOR, 0, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f},
	{	SOCK_RGBA, 0, "Diffuse",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "Spec",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 0, "AO",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_exec_material(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if(data && node->id) {
		ShadeResult shrnode;
		ShadeInput *shi;
		ShaderCallData *shcd= data;
		float col[4];
		
		shi= shcd->shi;
		shi->mat= (Material *)node->id;
		
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
		shi->har= shi->mat->har;
		
		/* write values */
		if(in[MAT_IN_COLOR]->hasinput)
			nodestack_get_vec(&shi->r, SOCK_VECTOR, in[MAT_IN_COLOR]);
		
		if(in[MAT_IN_SPEC]->hasinput)
			nodestack_get_vec(&shi->specr, SOCK_VECTOR, in[MAT_IN_SPEC]);
		
		if(in[MAT_IN_REFL]->hasinput)
			nodestack_get_vec(&shi->refl, SOCK_VALUE, in[MAT_IN_REFL]);
		
		/* retrieve normal */
		if(in[MAT_IN_NORMAL]->hasinput) {
			nodestack_get_vec(shi->vn, SOCK_VECTOR, in[MAT_IN_NORMAL]);
			normalize_v3(shi->vn);
		}
		else
			VECCOPY(shi->vn, shi->vno);
		
		/* custom option to flip normal */
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		
		if (node->type == SH_NODE_MATERIAL_EXT) {
			if(in[MAT_IN_MIR]->hasinput)
				nodestack_get_vec(&shi->mirr, SOCK_VECTOR, in[MAT_IN_MIR]);
			if(in[MAT_IN_AMB]->hasinput)
				nodestack_get_vec(&shi->amb, SOCK_VALUE, in[MAT_IN_AMB]);
			if(in[MAT_IN_EMIT]->hasinput)
				nodestack_get_vec(&shi->emit, SOCK_VALUE, in[MAT_IN_EMIT]);
			if(in[MAT_IN_SPECTRA]->hasinput)
				nodestack_get_vec(&shi->spectra, SOCK_VALUE, in[MAT_IN_SPECTRA]);
			if(in[MAT_IN_RAY_MIRROR]->hasinput)
				nodestack_get_vec(&shi->ray_mirror, SOCK_VALUE, in[MAT_IN_RAY_MIRROR]);
			if(in[MAT_IN_ALPHA]->hasinput)
				nodestack_get_vec(&shi->alpha, SOCK_VALUE, in[MAT_IN_ALPHA]);
			if(in[MAT_IN_TRANSLUCENCY]->hasinput)
				nodestack_get_vec(&shi->translucency, SOCK_VALUE, in[MAT_IN_TRANSLUCENCY]);			
		}
		
		shi->nodes= 1; /* temp hack to prevent trashadow recursion */
		node_shader_lamp_loop(shi, &shrnode);	/* clears shrnode */
		shi->nodes= 0;
		
		/* write to outputs */
		if(node->custom1 & SH_NODE_MAT_DIFF) {
			VECCOPY(col, shrnode.combined);
			if(!(node->custom1 & SH_NODE_MAT_SPEC)) {
				sub_v3_v3v3(col, col, shrnode.spec);
			}
		}
		else if(node->custom1 & SH_NODE_MAT_SPEC) {
			VECCOPY(col, shrnode.spec);
		}
		else
			col[0]= col[1]= col[2]= 0.0f;
		
		col[3]= shrnode.alpha;
		
		if(shi->do_preview)
			nodeAddToPreview(node, col, shi->xs, shi->ys);
		
		VECCOPY(out[MAT_OUT_COLOR]->vec, col);
		out[MAT_OUT_ALPHA]->vec[0]= shrnode.alpha;
		
		if(node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		
		VECCOPY(out[MAT_OUT_NORMAL]->vec, shi->vn);
		
		/* Extended material options */
		if (node->type == SH_NODE_MATERIAL_EXT) {
			/* Shadow, Reflect, Refract, Radiosity, Speed seem to cause problems inside
			 * a node tree :( */
			VECCOPY(out[MAT_OUT_DIFFUSE]->vec, shrnode.diff);
			VECCOPY(out[MAT_OUT_SPEC]->vec, shrnode.spec);
			VECCOPY(out[MAT_OUT_AO]->vec, shrnode.ao);
		}
		
		/* copy passes, now just active node */
		if(node->flag & NODE_ACTIVE_ID) {
			float combined[4], alpha;

			copy_v4_v4(combined, shcd->shr->combined);
			alpha= shcd->shr->alpha;

			*(shcd->shr)= shrnode;

			copy_v4_v4(shcd->shr->combined, combined);
			shcd->shr->alpha= alpha;
		}
	}
}


static void node_shader_init_material(bNode* node)
{
   node->custom1= SH_NODE_MAT_DIFF|SH_NODE_MAT_SPEC;
}

static int gpu_shader_material(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if(node->id) {
		GPUShadeInput shi;
		GPUShadeResult shr;

		GPU_shadeinput_set(mat, (Material*)node->id, &shi);

		/* write values */
		if(in[MAT_IN_COLOR].hasinput)
			shi.rgb = in[MAT_IN_COLOR].link;
		
		if(in[MAT_IN_SPEC].hasinput)
			shi.specrgb = in[MAT_IN_SPEC].link;
		
		if(in[MAT_IN_REFL].hasinput)
			shi.refl = in[MAT_IN_REFL].link;
		
		/* retrieve normal */
		if(in[MAT_IN_NORMAL].hasinput) {
			GPUNodeLink *tmp;
			shi.vn = in[MAT_IN_NORMAL].link;
			GPU_link(mat, "vec_math_normalize", shi.vn, &shi.vn, &tmp);
		}
		
		/* custom option to flip normal */
		if(node->custom1 & SH_NODE_MAT_NEG)
			GPU_link(mat, "vec_math_negate", shi.vn, &shi.vn);

		if (node->type == SH_NODE_MATERIAL_EXT) {
			if(in[MAT_IN_AMB].hasinput)
				shi.amb= in[MAT_IN_AMB].link;
			if(in[MAT_IN_EMIT].hasinput)
				shi.emit= in[MAT_IN_EMIT].link;
			if(in[MAT_IN_ALPHA].hasinput)
				shi.alpha= in[MAT_IN_ALPHA].link;
		}

		GPU_shaderesult_set(&shi, &shr); /* clears shr */
		
		/* write to outputs */
		if(node->custom1 & SH_NODE_MAT_DIFF) {
			if(node->custom1 & SH_NODE_MAT_SPEC)
				out[MAT_OUT_COLOR].link= shr.combined;
			else
				out[MAT_OUT_COLOR].link= shr.diff;
		}
		else if(node->custom1 & SH_NODE_MAT_SPEC) {
			out[MAT_OUT_COLOR].link= shr.spec;
		}
		else
			GPU_link(mat, "set_rgb_zero", &out[MAT_OUT_COLOR].link);

		GPU_link(mat, "mtex_alpha_to_col", out[MAT_OUT_COLOR].link, shr.alpha, &out[MAT_OUT_COLOR].link);

		out[MAT_OUT_ALPHA].link = shr.alpha; //
		
		if(node->custom1 & SH_NODE_MAT_NEG)
			GPU_link(mat, "vec_math_negate", shi.vn, &shi.vn);
		out[MAT_OUT_NORMAL].link = shi.vn;

		if (node->type == SH_NODE_MATERIAL_EXT) {
			out[MAT_OUT_DIFFUSE].link = shr.diff;
			out[MAT_OUT_SPEC].link = shr.spec;
		}

		return 1;
	}

	return 0;
}

bNodeType sh_node_material= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_MATERIAL,
	/* name        */	"Material",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_material_in,
	/* output sock */	sh_node_material_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_material,
	/* butfunc     */	NULL,
	/* initfunc    */	node_shader_init_material,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_material
};

bNodeType sh_node_material_ext= {
	/* *next,*prev */	NULL, NULL,
	/* type code   */	SH_NODE_MATERIAL_EXT,
	/* name        */	"Extended Material",
	/* width+range */	120, 80, 240,
	/* class+opts  */	NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW,
	/* input sock  */	sh_node_material_ext_in,
	/* output sock */	sh_node_material_ext_out,
	/* storage     */	"",
	/* execfunc    */	node_shader_exec_material,
	/* butfunc     */	NULL,
	/* initfunc    */	node_shader_init_material,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL, NULL, NULL,
	/* gpufunc     */	gpu_shader_material
};

