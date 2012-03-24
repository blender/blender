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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/nodes/node_shader_material.c
 *  \ingroup shdnodes
 */


#include "node_shader_util.h"

/* **************** MATERIAL ******************** */

static bNodeSocketTemplate sh_node_material_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Spec",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Refl",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_DIRECTION},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_material_out[]= {
	{	SOCK_RGBA, 0, "Color"},
	{	SOCK_FLOAT, 0, "Alpha"},
	{	SOCK_VECTOR, 0, "Normal"},
	{	-1, 0, ""	}
};

/* **************** EXTENDED MATERIAL ******************** */

static bNodeSocketTemplate sh_node_material_ext_in[]= {
	{	SOCK_RGBA, 1, "Color",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_RGBA, 1, "Spec",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Refl",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_VECTOR, 1, "Normal",	0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, PROP_DIRECTION},
	{	SOCK_RGBA, 1, "Mirror",		0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Ambient",	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, "Emit",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	SOCK_FLOAT, 1, "SpecTra",	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, "Ray Mirror",	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	SOCK_FLOAT, 1, "Alpha",		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_UNSIGNED},
	{	SOCK_FLOAT, 1, "Translucency",	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_material_ext_out[]= {
	{	SOCK_RGBA, 0, "Color"},
	{	SOCK_FLOAT, 0, "Alpha"},
	{	SOCK_VECTOR, 0, "Normal"},
	{	SOCK_RGBA, 0, "Diffuse"},
	{	SOCK_RGBA, 0, "Spec"},
	{	SOCK_RGBA, 0, "AO"},
	{	-1, 0, ""	}
};

static void node_shader_exec_material(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	if (data && node->id) {
		ShadeResult shrnode;
		ShadeInput *shi;
		ShaderCallData *shcd= data;
		float col[4];
		bNodeSocket *sock;
		char hasinput[NUM_MAT_IN]= {'\0'};
		int i;
		
		/* note: cannot use the in[]->hasinput flags directly, as these are not necessarily
		 * the constant input stack values (e.g. in case material node is inside a group).
		 * we just want to know if a node input uses external data or the material setting.
		 * this is an ugly hack, but so is this node as a whole.
		 */
		for (sock=node->inputs.first, i=0; sock; sock=sock->next, ++i)
			hasinput[i] = (sock->link != NULL);
		
		shi= shcd->shi;
		shi->mat= (Material *)node->id;
		
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		memcpy(&shi->r, &shi->mat->r, 23*sizeof(float));
		shi->har= shi->mat->har;
		
		/* write values */
		if (hasinput[MAT_IN_COLOR])
			nodestack_get_vec(&shi->r, SOCK_VECTOR, in[MAT_IN_COLOR]);
		
		if (hasinput[MAT_IN_SPEC])
			nodestack_get_vec(&shi->specr, SOCK_VECTOR, in[MAT_IN_SPEC]);
		
		if (hasinput[MAT_IN_REFL])
			nodestack_get_vec(&shi->refl, SOCK_FLOAT, in[MAT_IN_REFL]);
		
		/* retrieve normal */
		if (hasinput[MAT_IN_NORMAL]) {
			nodestack_get_vec(shi->vn, SOCK_VECTOR, in[MAT_IN_NORMAL]);
			normalize_v3(shi->vn);
		}
		else
			copy_v3_v3(shi->vn, shi->vno);
		
		/* custom option to flip normal */
		if (node->custom1 & SH_NODE_MAT_NEG) {
			negate_v3(shi->vn);
		}
		
		if (node->type == SH_NODE_MATERIAL_EXT) {
			if (hasinput[MAT_IN_MIR])
				nodestack_get_vec(&shi->mirr, SOCK_VECTOR, in[MAT_IN_MIR]);
			if (hasinput[MAT_IN_AMB])
				nodestack_get_vec(&shi->amb, SOCK_FLOAT, in[MAT_IN_AMB]);
			if (hasinput[MAT_IN_EMIT])
				nodestack_get_vec(&shi->emit, SOCK_FLOAT, in[MAT_IN_EMIT]);
			if (hasinput[MAT_IN_SPECTRA])
				nodestack_get_vec(&shi->spectra, SOCK_FLOAT, in[MAT_IN_SPECTRA]);
			if (hasinput[MAT_IN_RAY_MIRROR])
				nodestack_get_vec(&shi->ray_mirror, SOCK_FLOAT, in[MAT_IN_RAY_MIRROR]);
			if (hasinput[MAT_IN_ALPHA])
				nodestack_get_vec(&shi->alpha, SOCK_FLOAT, in[MAT_IN_ALPHA]);
			if (hasinput[MAT_IN_TRANSLUCENCY])
				nodestack_get_vec(&shi->translucency, SOCK_FLOAT, in[MAT_IN_TRANSLUCENCY]);			
		}
		
		shi->nodes= 1; /* temp hack to prevent trashadow recursion */
		node_shader_lamp_loop(shi, &shrnode);	/* clears shrnode */
		shi->nodes= 0;
		
		/* write to outputs */
		if (node->custom1 & SH_NODE_MAT_DIFF) {
			copy_v3_v3(col, shrnode.combined);
			if (!(node->custom1 & SH_NODE_MAT_SPEC)) {
				sub_v3_v3(col, shrnode.spec);
			}
		}
		else if (node->custom1 & SH_NODE_MAT_SPEC) {
			copy_v3_v3(col, shrnode.spec);
		}
		else
			col[0]= col[1]= col[2]= 0.0f;
		
		col[3]= shrnode.alpha;
		
		if (shi->do_preview)
			nodeAddToPreview(node, col, shi->xs, shi->ys, shi->do_manage);
		
		copy_v3_v3(out[MAT_OUT_COLOR]->vec, col);
		out[MAT_OUT_ALPHA]->vec[0]= shrnode.alpha;
		
		if (node->custom1 & SH_NODE_MAT_NEG) {
			shi->vn[0]= -shi->vn[0];
			shi->vn[1]= -shi->vn[1];
			shi->vn[2]= -shi->vn[2];
		}
		
		copy_v3_v3(out[MAT_OUT_NORMAL]->vec, shi->vn);
		
		/* Extended material options */
		if (node->type == SH_NODE_MATERIAL_EXT) {
			/* Shadow, Reflect, Refract, Radiosity, Speed seem to cause problems inside
			 * a node tree :( */
			copy_v3_v3(out[MAT_OUT_DIFFUSE]->vec, shrnode.diff);
			copy_v3_v3(out[MAT_OUT_SPEC]->vec, shrnode.spec);
			copy_v3_v3(out[MAT_OUT_AO]->vec, shrnode.ao);
		}
		
		/* copy passes, now just active node */
		if (node->flag & NODE_ACTIVE_ID) {
			float combined[4], alpha;

			copy_v4_v4(combined, shcd->shr->combined);
			alpha= shcd->shr->alpha;

			*(shcd->shr)= shrnode;

			copy_v4_v4(shcd->shr->combined, combined);
			shcd->shr->alpha= alpha;
		}
	}
}


static void node_shader_init_material(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	node->custom1= SH_NODE_MAT_DIFF|SH_NODE_MAT_SPEC;
}

/* XXX this is also done as a local static function in gpu_codegen.c,
 * but we need this to hack around the crappy material node.
 */
static GPUNodeLink *gpu_get_input_link(GPUNodeStack *in)
{
	if (in->link)
		return in->link;
	else
		return GPU_uniform(in->vec);
}

static int gpu_shader_material(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	if (node->id) {
		GPUShadeInput shi;
		GPUShadeResult shr;
		bNodeSocket *sock;
		char hasinput[NUM_MAT_IN]= {'\0'};
		int i;
		
		/* note: cannot use the in[]->hasinput flags directly, as these are not necessarily
		 * the constant input stack values (e.g. in case material node is inside a group).
		 * we just want to know if a node input uses external data or the material setting.
		 */
		for (sock=node->inputs.first, i=0; sock; sock=sock->next, ++i)
			hasinput[i] = (sock->link != NULL);

		GPU_shadeinput_set(mat, (Material*)node->id, &shi);

		/* write values */
		if (hasinput[MAT_IN_COLOR])
			shi.rgb = gpu_get_input_link(&in[MAT_IN_COLOR]);
		
		if (hasinput[MAT_IN_SPEC])
			shi.specrgb = gpu_get_input_link(&in[MAT_IN_SPEC]);
		
		if (hasinput[MAT_IN_REFL])
			shi.refl = gpu_get_input_link(&in[MAT_IN_REFL]);
		
		/* retrieve normal */
		if (hasinput[MAT_IN_NORMAL]) {
			GPUNodeLink *tmp;
			shi.vn = gpu_get_input_link(&in[MAT_IN_NORMAL]);
			GPU_link(mat, "vec_math_normalize", shi.vn, &shi.vn, &tmp);
		}
		
		/* custom option to flip normal */
		if (node->custom1 & SH_NODE_MAT_NEG)
			GPU_link(mat, "vec_math_negate", shi.vn, &shi.vn);

		if (node->type == SH_NODE_MATERIAL_EXT) {
			if (hasinput[MAT_IN_AMB])
				shi.amb= gpu_get_input_link(&in[MAT_IN_AMB]);
			if (hasinput[MAT_IN_EMIT])
				shi.emit= gpu_get_input_link(&in[MAT_IN_EMIT]);
			if (hasinput[MAT_IN_ALPHA])
				shi.alpha= gpu_get_input_link(&in[MAT_IN_ALPHA]);
		}

		GPU_shaderesult_set(&shi, &shr); /* clears shr */
		
		/* write to outputs */
		if (node->custom1 & SH_NODE_MAT_DIFF) {
			out[MAT_OUT_COLOR].link= shr.combined;

			if (!(node->custom1 & SH_NODE_MAT_SPEC)) {
				GPUNodeLink *link;
				GPU_link(mat, "vec_math_sub", shr.combined, shr.spec, &out[MAT_OUT_COLOR].link, &link);
			}
		}
		else if (node->custom1 & SH_NODE_MAT_SPEC) {
			out[MAT_OUT_COLOR].link= shr.spec;
		}
		else
			GPU_link(mat, "set_rgb_zero", &out[MAT_OUT_COLOR].link);

		GPU_link(mat, "mtex_alpha_to_col", out[MAT_OUT_COLOR].link, shr.alpha, &out[MAT_OUT_COLOR].link);

		out[MAT_OUT_ALPHA].link = shr.alpha; //
		
		if (node->custom1 & SH_NODE_MAT_NEG)
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

void register_node_type_sh_material(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_MATERIAL, "Material", NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_material_in, sh_node_material_out);
	node_type_size(&ntype, 120, 80, 240);
	node_type_init(&ntype, node_shader_init_material);
	node_type_exec(&ntype, node_shader_exec_material);
	node_type_gpu(&ntype, gpu_shader_material);

	nodeRegisterType(ttype, &ntype);
}


void register_node_type_sh_material_ext(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_MATERIAL_EXT, "Extended Material", NODE_CLASS_INPUT, NODE_OPTIONS|NODE_PREVIEW);
	node_type_compatibility(&ntype, NODE_OLD_SHADING);
	node_type_socket_templates(&ntype, sh_node_material_ext_in, sh_node_material_ext_out);
	node_type_size(&ntype, 120, 80, 240);
	node_type_init(&ntype, node_shader_init_material);
	node_type_exec(&ntype, node_shader_exec_material);
	node_type_gpu(&ntype, gpu_shader_material);

	nodeRegisterType(ttype, &ntype);
}
