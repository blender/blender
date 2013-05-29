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
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/texture/nodes/node_texture_proc.c
 *  \ingroup texnodes
 */


#include "node_texture_util.h"
#include "NOD_texture.h"

#include "RE_shader_ext.h"

/* 
 * In this file: wrappers to use procedural textures as nodes
 */


static bNodeSocketTemplate outputs_both[] = {
	{ SOCK_RGBA, 0, N_("Color"),  1.0f, 0.0f, 0.0f, 1.0f },
	{ SOCK_VECTOR, 0, N_("Normal"), 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, PROP_DIRECTION },
	{ -1, 0, "" }
};
static bNodeSocketTemplate outputs_color_only[] = {
	{ SOCK_RGBA, 0, N_("Color") },
	{ -1, 0, "" }
};

/* Inputs common to all, #defined because nodes will need their own inputs too */
#define I 2 /* count */
#define COMMON_INPUTS \
	{ SOCK_RGBA, 1, "Color 1", 0.0f, 0.0f, 0.0f, 1.0f }, \
	{ SOCK_RGBA, 1, "Color 2", 1.0f, 1.0f, 1.0f, 1.0f }

/* Calls multitex and copies the result to the outputs. Called by xxx_exec, which handles inputs. */
static void do_proc(float *result, TexParams *p, const float col1[4], const float col2[4], char is_normal, Tex *tex, const short thread)
{
	TexResult texres;
	int textype;
	
	if (is_normal) {
		texres.nor = result;
	}
	else
		texres.nor = NULL;
	
	textype = multitex_nodes(tex, p->co, p->dxt, p->dyt, p->osatex,
	                         &texres, thread, 0, p->shi, p->mtex, NULL);
	
	if (is_normal)
		return;
	
	if (textype & TEX_RGB) {
		copy_v4_v4(result, &texres.tr);
	}
	else {
		copy_v4_v4(result, col1);
		ramp_blend(MA_RAMP_BLEND, result, texres.tin, col2);
	}
}

typedef void (*MapFn) (Tex *tex, bNodeStack **in, TexParams *p, const short thread);

static void texfn(
	float *result, 
	TexParams *p,
	bNode *node, 
	bNodeStack **in,
	char is_normal, 
	MapFn map_inputs,
	short thread)
{
	Tex tex = *((Tex *)(node->storage));
	float col1[4], col2[4];
	tex_input_rgba(col1, in[0], p, thread);
	tex_input_rgba(col2, in[1], p, thread);
	
	map_inputs(&tex, in, p, thread);
	
	do_proc(result, p, col1, col2, is_normal, &tex, thread);
}

static int count_outputs(bNode *node)
{
	bNodeSocket *sock;
	int num = 0;
	for (sock = node->outputs.first; sock; sock = sock->next) {
		num++;
	}
	return num;
}

/* Boilerplate generators */

#define ProcNoInputs(name) \
		static void name##_map_inputs(Tex *UNUSED(tex), bNodeStack **UNUSED(in), TexParams *UNUSED(p), short UNUSED(thread)) \
		{}

#define ProcDef(name) \
	static void name##_colorfn(float *result, TexParams *p, bNode *node, bNodeStack **in, short thread)  \
	{                                                                                                    \
		texfn(result, p, node, in, 0, &name##_map_inputs, thread);                                       \
	}                                                                                                    \
	static void name##_normalfn(float *result, TexParams *p, bNode *node, bNodeStack **in, short thread) \
	{                                                                                                    \
		texfn(result, p, node, in, 1, &name##_map_inputs, thread);                                       \
	}                                                                                                    \
	static void name##_exec(void *data, int UNUSED(thread), bNode *node, bNodeExecData *execdata, bNodeStack **in, bNodeStack **out) \
	{                                                                                                    \
		int outs = count_outputs(node);                                                                  \
		if (outs >= 1) tex_output(node, execdata, in, out[0], &name##_colorfn, data);                    \
		if (outs >= 2) tex_output(node, execdata, in, out[1], &name##_normalfn, data);                   \
	}


/* --- VORONOI -- */
static bNodeSocketTemplate voronoi_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("W1"), 1.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("W2"), 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("W3"), 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	{ SOCK_FLOAT, 1, N_("W4"), 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f, PROP_NONE },
	
	{ SOCK_FLOAT, 1, N_("iScale"), 1.0f, 0.0f, 0.0f, 0.0f,    0.01f,  10.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Size"),   0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 4.0f, PROP_UNSIGNED },
	
	{ -1, 0, "" }
};
static void voronoi_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->vn_w1 = tex_input_value(in[I + 0], p, thread);
	tex->vn_w2 = tex_input_value(in[I + 1], p, thread);
	tex->vn_w3 = tex_input_value(in[I + 2], p, thread);
	tex->vn_w4 = tex_input_value(in[I + 3], p, thread);
	
	tex->ns_outscale = tex_input_value(in[I + 4], p, thread);
	tex->noisesize   = tex_input_value(in[I + 5], p, thread);
}
ProcDef(voronoi)

/* --- BLEND -- */
static bNodeSocketTemplate blend_inputs[] = {
	COMMON_INPUTS,
	{ -1, 0, "" }
};
ProcNoInputs(blend)
ProcDef(blend)

/* -- MAGIC -- */
static bNodeSocketTemplate magic_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f, 0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void magic_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->turbul = tex_input_value(in[I + 0], p, thread);
}
ProcDef(magic)

/* --- MARBLE --- */
static bNodeSocketTemplate marble_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void marble_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I + 0], p, thread);
	tex->turbul    = tex_input_value(in[I + 1], p, thread);
}
ProcDef(marble)

/* --- CLOUDS --- */
static bNodeSocketTemplate clouds_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void clouds_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I + 0], p, thread);
}
ProcDef(clouds)

/* --- DISTORTED NOISE --- */
static bNodeSocketTemplate distnoise_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f,  2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Distortion"), 1.00f, 0.0f, 0.0f, 0.0f,   0.0000f, 10.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void distnoise_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize   = tex_input_value(in[I + 0], p, thread);
	tex->dist_amount = tex_input_value(in[I + 1], p, thread);
}
ProcDef(distnoise)

/* --- WOOD --- */
static bNodeSocketTemplate wood_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void wood_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I + 0], p, thread);
	tex->turbul    = tex_input_value(in[I + 1], p, thread);
}
ProcDef(wood)

/* --- MUSGRAVE --- */
static bNodeSocketTemplate musgrave_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("H"),          1.0f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Lacunarity"), 2.0f, 0.0f, 0.0f, 0.0f,   0.0f,    6.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Octaves"),    2.0f, 0.0f, 0.0f, 0.0f,   0.0f,    8.0f, PROP_UNSIGNED },
	
	{ SOCK_FLOAT, 1, N_("iScale"),     1.0f,  0.0f, 0.0f, 0.0f,  0.0f,   10.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,  0.0001f, 2.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void musgrave_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->mg_H          = tex_input_value(in[I + 0], p, thread);
	tex->mg_lacunarity = tex_input_value(in[I + 1], p, thread);
	tex->mg_octaves    = tex_input_value(in[I + 2], p, thread);
	tex->ns_outscale   = tex_input_value(in[I + 3], p, thread);
	tex->noisesize     = tex_input_value(in[I + 4], p, thread);
}
ProcDef(musgrave)

/* --- NOISE --- */
static bNodeSocketTemplate noise_inputs[] = {
	COMMON_INPUTS,
	{ -1, 0, "" }
};
ProcNoInputs(noise)
ProcDef(noise)

/* --- STUCCI --- */
static bNodeSocketTemplate stucci_inputs[] = {
	COMMON_INPUTS,
	{ SOCK_FLOAT, 1, N_("Size"),       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f, PROP_UNSIGNED },
	{ SOCK_FLOAT, 1, N_("Turbulence"), 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f, PROP_UNSIGNED },
	{ -1, 0, "" }
};
static void stucci_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I + 0], p, thread);
	tex->turbul    = tex_input_value(in[I + 1], p, thread);
}
ProcDef(stucci)

/* --- */

static void init(bNodeTree *UNUSED(ntree), bNode *node)
{
	Tex *tex = MEM_callocN(sizeof(Tex), "Tex");
	node->storage = tex;
	
	default_tex(tex);
	tex->type = node->type - TEX_NODE_PROC;
	
	if (tex->type == TEX_WOOD)
		tex->stype = TEX_BANDNOISE;
	
}

/* Node type definitions */
#define TexDef(TEXTYPE, outputs, name, Name) \
void register_node_type_tex_proc_##name(void) \
{ \
	static bNodeType ntype; \
	\
	tex_node_type_base(&ntype, TEX_NODE_PROC+TEXTYPE, Name, NODE_CLASS_TEXTURE, NODE_PREVIEW); \
	node_type_socket_templates(&ntype, name##_inputs, outputs); \
	node_type_size_preset(&ntype, NODE_SIZE_MIDDLE); \
	node_type_init(&ntype, init); \
	node_type_storage(&ntype, "Tex", node_free_standard_storage, node_copy_standard_storage); \
	node_type_exec(&ntype, NULL, NULL, name##_exec); \
	\
	nodeRegisterType(&ntype); \
}
	
#define C outputs_color_only
#define CV outputs_both
	
TexDef(TEX_VORONOI,   CV, voronoi,   "Voronoi"  )
TexDef(TEX_BLEND,     C,  blend,     "Blend"    )
TexDef(TEX_MAGIC,     C,  magic,     "Magic"    )
TexDef(TEX_MARBLE,    CV, marble,    "Marble"   )
TexDef(TEX_CLOUDS,    CV, clouds,    "Clouds"   )
TexDef(TEX_WOOD,      CV, wood,      "Wood"     )
TexDef(TEX_MUSGRAVE,  CV, musgrave,  "Musgrave" )
TexDef(TEX_NOISE,     C,  noise,     "Noise"    )
TexDef(TEX_STUCCI,    CV, stucci,    "Stucci"   )
TexDef(TEX_DISTNOISE, CV, distnoise, "Distorted Noise" )
