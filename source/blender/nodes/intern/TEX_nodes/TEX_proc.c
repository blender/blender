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

#include "../TEX_util.h"
#include "RE_shader_ext.h"

/* 
	In this file: wrappers to use procedural textures as nodes
*/


static bNodeSocketType outputs_both[]= {
	{ SOCK_RGBA, 0, "Color",  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ SOCK_VECTOR, 0, "Normal", 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};
static bNodeSocketType outputs_color_only[]= {
	{ SOCK_RGBA, 0, "Color",  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -1, 0, "" }
};

/* Inputs common to all, #defined because nodes will need their own inputs too */
#define I 2 /* count */
#define COMMON_INPUTS \
	{ SOCK_RGBA, 1, "Color 1", 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f }, \
	{ SOCK_RGBA, 1, "Color 2", 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f }

/* Calls multitex and copies the result to the outputs. Called by xxx_exec, which handles inputs. */
static void do_proc(float *result, TexParams *p, float *col1, float *col2, char is_normal, Tex *tex, short thread)
{
	TexResult texres;
	int textype;
	
	if(is_normal) {
		texres.nor = result;
	}
	else
		texres.nor = NULL;
	
	textype = multitex_nodes(tex, p->co, p->dxt, p->dyt, p->osatex,
		&texres, thread, 0, p->shi, p->mtex);
	
	if(is_normal)
		return;
	
	if(textype & TEX_RGB) {
		QUATCOPY(result, &texres.tr);
	}
	else {
		QUATCOPY(result, col1);
		ramp_blend(MA_RAMP_BLEND, result, result+1, result+2, texres.tin, col2);
	}
}

typedef void (*MapFn) (Tex *tex, bNodeStack **in, TexParams *p, short thread);

static void texfn(
	float *result, 
	TexParams *p,
	bNode *node, 
	bNodeStack **in,
	char is_normal, 
	MapFn map_inputs,
	short thread)
{
	Tex tex = *((Tex*)(node->storage));
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
	for(sock= node->outputs.first; sock; sock= sock->next) {
		num++;
	}
	return num;
}

/* Boilerplate generators */

#define ProcNoInputs(name) \
		static void name##_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread) \
		{}

#define ProcDef(name) \
		static void name##_colorfn(float *result, TexParams *p, bNode *node, bNodeStack **in, short thread)  \
		{                                                                                                    \
				texfn(result, p, node, in, 0, &name##_map_inputs, thread);                               \
		}                                                                                                    \
		static void name##_normalfn(float *result, TexParams *p, bNode *node, bNodeStack **in, short thread) \
		{                                                                                                    \
				texfn(result, p, node, in, 1, &name##_map_inputs, thread);                               \
		}                                                                                                    \
		static void name##_exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)                  \
		{                                                                                                    \
				int outs = count_outputs(node);                                                              \
				if(outs >= 1) tex_output(node, in, out[0], &name##_colorfn, data);                                 \
				if(outs >= 2) tex_output(node, in, out[1], &name##_normalfn, data);                                \
		}


/* --- VORONOI -- */
static bNodeSocketType voronoi_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "W1", 1.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f },
	{ SOCK_VALUE, 1, "W2", 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f },
	{ SOCK_VALUE, 1, "W3", 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f },
	{ SOCK_VALUE, 1, "W4", 0.0f, 0.0f, 0.0f, 0.0f,   -2.0f, 2.0f },
	
	{ SOCK_VALUE, 1, "iScale", 1.0f, 0.0f, 0.0f, 0.0f,    0.01f,  10.0f },
	{ SOCK_VALUE, 1, "Size",   0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 4.0f },
	
	{ -1, 0, "" }
};
static void voronoi_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->vn_w1 = tex_input_value(in[I+0], p, thread);
	tex->vn_w2 = tex_input_value(in[I+1], p, thread);
	tex->vn_w3 = tex_input_value(in[I+2], p, thread);
	tex->vn_w4 = tex_input_value(in[I+3], p, thread);
	
	tex->ns_outscale = tex_input_value(in[I+4], p, thread);
	tex->noisesize   = tex_input_value(in[I+5], p, thread);
}
ProcDef(voronoi)

/* --- BLEND -- */
static bNodeSocketType blend_inputs[]= {
	COMMON_INPUTS,
	{ -1, 0, "" }
};
ProcNoInputs(blend)
ProcDef(blend)

/* -- MAGIC -- */
static bNodeSocketType magic_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "Turbulence", 5.0f, 0.0f, 0.0f, 0.0f,   0.0f, 200.0f },
	{ -1, 0, "" }
};
static void magic_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->turbul = tex_input_value(in[I+0], p, thread);
}
ProcDef(magic)

/* --- MARBLE --- */
static bNodeSocketType marble_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "Size",       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f },
	{ SOCK_VALUE, 1, "Turbulence", 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f },
	{ -1, 0, "" }
};
static void marble_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I+0], p, thread);
	tex->turbul    = tex_input_value(in[I+1], p, thread);
}
ProcDef(marble)

/* --- CLOUDS --- */
static bNodeSocketType clouds_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "Size",       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f },
	{ -1, 0, "" }
};
static void clouds_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I+0], p, thread);
}
ProcDef(clouds)

/* --- DISTORTED NOISE --- */
static bNodeSocketType distnoise_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "Size",       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f,  2.0f },
	{ SOCK_VALUE, 1, "Distortion", 1.00f, 0.0f, 0.0f, 0.0f,   0.0000f, 10.0f },
	{ -1, 0, "" }
};
static void distnoise_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize   = tex_input_value(in[I+0], p, thread);
	tex->dist_amount = tex_input_value(in[I+1], p, thread);
}
ProcDef(distnoise)

/* --- WOOD --- */
static bNodeSocketType wood_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "Size",       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f },
	{ SOCK_VALUE, 1, "Turbulence", 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f },
	{ -1, 0, "" }
};
static void wood_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I+0], p, thread);
	tex->turbul    = tex_input_value(in[I+1], p, thread);
}
ProcDef(wood)

/* --- MUSGRAVE --- */
static bNodeSocketType musgrave_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "H",          1.0f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f },
	{ SOCK_VALUE, 1, "Lacunarity", 2.0f, 0.0f, 0.0f, 0.0f,   0.0f,    6.0f },
	{ SOCK_VALUE, 1, "Octaves",    2.0f, 0.0f, 0.0f, 0.0f,   0.0f,    8.0f },
	
	{ SOCK_VALUE, 1, "iScale",     1.0f,  0.0f, 0.0f, 0.0f,  0.0f,   10.0f },
	{ SOCK_VALUE, 1, "Size",       0.25f, 0.0f, 0.0f, 0.0f,  0.0001f, 2.0f },
	{ -1, 0, "" }
};
static void musgrave_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->mg_H          = tex_input_value(in[I+0], p, thread);
	tex->mg_lacunarity = tex_input_value(in[I+1], p, thread);
	tex->mg_octaves    = tex_input_value(in[I+2], p, thread);
	tex->ns_outscale   = tex_input_value(in[I+3], p, thread);
	tex->noisesize     = tex_input_value(in[I+4], p, thread);
}
ProcDef(musgrave)

/* --- NOISE --- */
static bNodeSocketType noise_inputs[]= {
	COMMON_INPUTS,
	{ -1, 0, "" }
};
ProcNoInputs(noise)
ProcDef(noise)

/* --- STUCCI --- */
static bNodeSocketType stucci_inputs[]= {
	COMMON_INPUTS,
	{ SOCK_VALUE, 1, "Size",       0.25f, 0.0f, 0.0f, 0.0f,   0.0001f, 2.0f },
	{ SOCK_VALUE, 1, "Turbulence", 5.0f,  0.0f, 0.0f, 0.0f,   0.0f, 200.0f },
	{ -1, 0, "" }
};
static void stucci_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
	tex->noisesize = tex_input_value(in[I+0], p, thread);
	tex->turbul    = tex_input_value(in[I+1], p, thread);
}
ProcDef(stucci)

/* --- */

static void init(bNode *node)
{
	Tex *tex = MEM_callocN(sizeof(Tex), "Tex");
	node->storage= tex;
	
	default_tex(tex);
	tex->type = node->type - TEX_NODE_PROC;
	
	if(tex->type == TEX_WOOD)
		tex->stype = TEX_BANDNOISE;
	
}

/* Node type definitions */
#define TexDef(TEXTYPE, outputs, name, Name) \
	{ NULL, NULL, TEX_NODE_PROC+TEXTYPE, Name, 140,80,140, NODE_CLASS_TEXTURE, \
	NODE_OPTIONS | NODE_PREVIEW, name##_inputs, outputs, "Tex", name##_exec, NULL, init, \
	node_free_standard_storage, node_copy_standard_storage, NULL }
	
#define C outputs_color_only
#define CV outputs_both
	
bNodeType tex_node_proc_voronoi   = TexDef(TEX_VORONOI,   CV, voronoi,   "Voronoi"  );
bNodeType tex_node_proc_blend     = TexDef(TEX_BLEND,     C,  blend,     "Blend"    );
bNodeType tex_node_proc_magic     = TexDef(TEX_MAGIC,     C,  magic,     "Magic"    );
bNodeType tex_node_proc_marble    = TexDef(TEX_MARBLE,    CV, marble,    "Marble"   );
bNodeType tex_node_proc_clouds    = TexDef(TEX_CLOUDS,    CV, clouds,    "Clouds"   );
bNodeType tex_node_proc_wood      = TexDef(TEX_WOOD,      CV, wood,      "Wood"     );
bNodeType tex_node_proc_musgrave  = TexDef(TEX_MUSGRAVE,  CV, musgrave,  "Musgrave" );
bNodeType tex_node_proc_noise     = TexDef(TEX_NOISE,     C,  noise,     "Noise"    );
bNodeType tex_node_proc_stucci    = TexDef(TEX_STUCCI,    CV, stucci,    "Stucci"   );
bNodeType tex_node_proc_distnoise = TexDef(TEX_DISTNOISE, CV, distnoise, "Distorted Noise" );

