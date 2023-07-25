/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "NOD_texture.h"
#include "node_texture_util.hh"

#include "RE_texture.h"

/*
 * In this file: wrappers to use procedural textures as nodes
 */

static bNodeSocketTemplate outputs_both[] = {{SOCK_RGBA, N_("Color"), 1.0f, 0.0f, 0.0f, 1.0f},
                                             {-1, ""}};
static bNodeSocketTemplate outputs_color_only[] = {{SOCK_RGBA, N_("Color")}, {-1, ""}};

/* Inputs common to all, #defined because nodes will need their own inputs too */
#define I 2 /* count */
#define COMMON_INPUTS \
  {SOCK_RGBA, "Color 1", 0.0f, 0.0f, 0.0f, 1.0f}, \
  { \
    SOCK_RGBA, "Color 2", 1.0f, 1.0f, 1.0f, 1.0f \
  }

/* Calls multitex and copies the result to the outputs.
 * Called by xxx_exec, which handles inputs. */
static void do_proc(float *result,
                    TexParams *p,
                    const float col1[4],
                    const float col2[4],
                    Tex *tex,
                    const short thread)
{
  TexResult texres;
  int textype;

  textype = multitex_nodes(
      tex, p->co, p->dxt, p->dyt, p->osatex, &texres, thread, 0, p->mtex, nullptr);

  if (textype & TEX_RGB) {
    copy_v4_v4(result, texres.trgba);
  }
  else {
    copy_v4_v4(result, col1);
    ramp_blend(MA_RAMP_BLEND, result, texres.tin, col2);
  }
}

using MapFn = void (*)(Tex *tex, bNodeStack **in, TexParams *p, const short thread);

static void texfn(
    float *result, TexParams *p, bNode *node, bNodeStack **in, MapFn map_inputs, short thread)
{
  Tex tex = blender::dna::shallow_copy(*((Tex *)(node->storage)));
  float col1[4], col2[4];
  tex_input_rgba(col1, in[0], p, thread);
  tex_input_rgba(col2, in[1], p, thread);

  map_inputs(&tex, in, p, thread);

  do_proc(result, p, col1, col2, &tex, thread);
}

static int count_outputs(bNode *node)
{
  bNodeSocket *sock;
  int num = 0;
  for (sock = static_cast<bNodeSocket *>(node->outputs.first); sock; sock = sock->next) {
    num++;
  }
  return num;
}

/* Boilerplate generators */

#define ProcNoInputs(name) \
  static void name##_map_inputs( \
      Tex * /*tex*/, bNodeStack ** /*in*/, TexParams * /*p*/, short /*thread*/) \
  { \
  }

#define ProcDef(name) \
  static void name##_colorfn( \
      float *result, TexParams *p, bNode *node, bNodeStack **in, short thread) \
  { \
    texfn(result, p, node, in, &name##_map_inputs, thread); \
  } \
  static void name##_exec(void *data, \
                          int /*thread*/, \
                          bNode *node, \
                          bNodeExecData *execdata, \
                          bNodeStack **in, \
                          bNodeStack **out) \
  { \
    int outs = count_outputs(node); \
    if (outs >= 1) { \
      tex_output(node, execdata, in, out[0], &name##_colorfn, static_cast<TexCallData *>(data)); \
    } \
  }

/* --- VORONOI -- */
static bNodeSocketTemplate voronoi_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("W1"), 1.0f, 0.0f, 0.0f, 0.0f, -2.0f, 2.0f, PROP_NONE},
    {SOCK_FLOAT, N_("W2"), 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 2.0f, PROP_NONE},
    {SOCK_FLOAT, N_("W3"), 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 2.0f, PROP_NONE},
    {SOCK_FLOAT, N_("W4"), 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 2.0f, PROP_NONE},

    {SOCK_FLOAT, N_("iScale"), 1.0f, 0.0f, 0.0f, 0.0f, 0.01f, 10.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 4.0f, PROP_UNSIGNED},

    {-1, ""}};
static void voronoi_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->vn_w1 = tex_input_value(in[I + 0], p, thread);
  tex->vn_w2 = tex_input_value(in[I + 1], p, thread);
  tex->vn_w3 = tex_input_value(in[I + 2], p, thread);
  tex->vn_w4 = tex_input_value(in[I + 3], p, thread);

  tex->ns_outscale = tex_input_value(in[I + 4], p, thread);
  tex->noisesize = tex_input_value(in[I + 5], p, thread);
}
ProcDef(voronoi);

/* --- BLEND -- */
static bNodeSocketTemplate blend_inputs[] = {COMMON_INPUTS, {-1, ""}};
ProcNoInputs(blend);
ProcDef(blend);

/* -- MAGIC -- */
static bNodeSocketTemplate magic_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("Turbulence"), 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f, PROP_UNSIGNED},
    {-1, ""}};
static void magic_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->turbul = tex_input_value(in[I + 0], p, thread);
}
ProcDef(magic);

/* --- MARBLE --- */
static bNodeSocketTemplate marble_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Turbulence"), 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f, PROP_UNSIGNED},
    {-1, ""}};
static void marble_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->noisesize = tex_input_value(in[I + 0], p, thread);
  tex->turbul = tex_input_value(in[I + 1], p, thread);
}
ProcDef(marble);

/* --- CLOUDS --- */
static bNodeSocketTemplate clouds_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {-1, ""}};
static void clouds_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->noisesize = tex_input_value(in[I + 0], p, thread);
}
ProcDef(clouds);

/* --- DISTORTED NOISE --- */
static bNodeSocketTemplate distnoise_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Distortion"), 1.00f, 0.0f, 0.0f, 0.0f, 0.0000f, 10.0f, PROP_UNSIGNED},
    {-1, ""}};
static void distnoise_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->noisesize = tex_input_value(in[I + 0], p, thread);
  tex->dist_amount = tex_input_value(in[I + 1], p, thread);
}
ProcDef(distnoise);

/* --- WOOD --- */
static bNodeSocketTemplate wood_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Turbulence"), 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f, PROP_UNSIGNED},
    {-1, ""}};
static void wood_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->noisesize = tex_input_value(in[I + 0], p, thread);
  tex->turbul = tex_input_value(in[I + 1], p, thread);
}
ProcDef(wood);

/* --- MUSGRAVE --- */
static bNodeSocketTemplate musgrave_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("H"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Lacunarity"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Octaves"), 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 8.0f, PROP_UNSIGNED},

    {SOCK_FLOAT, N_("iScale"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {-1, ""}};
static void musgrave_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->mg_H = tex_input_value(in[I + 0], p, thread);
  tex->mg_lacunarity = tex_input_value(in[I + 1], p, thread);
  tex->mg_octaves = tex_input_value(in[I + 2], p, thread);
  tex->ns_outscale = tex_input_value(in[I + 3], p, thread);
  tex->noisesize = tex_input_value(in[I + 4], p, thread);
}
ProcDef(musgrave);

/* --- NOISE --- */
static bNodeSocketTemplate noise_inputs[] = {COMMON_INPUTS, {-1, ""}};
ProcNoInputs(noise);
ProcDef(noise);

/* --- STUCCI --- */
static bNodeSocketTemplate stucci_inputs[] = {
    COMMON_INPUTS,
    {SOCK_FLOAT, N_("Size"), 0.25f, 0.0f, 0.0f, 0.0f, 0.0001f, 2.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Turbulence"), 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f, PROP_UNSIGNED},
    {-1, ""}};
static void stucci_map_inputs(Tex *tex, bNodeStack **in, TexParams *p, short thread)
{
  tex->noisesize = tex_input_value(in[I + 0], p, thread);
  tex->turbul = tex_input_value(in[I + 1], p, thread);
}
ProcDef(stucci);

/* --- */

static void init(bNodeTree * /*ntree*/, bNode *node)
{
  Tex *tex = static_cast<Tex *>(MEM_callocN(sizeof(Tex), "Tex"));
  node->storage = tex;

  BKE_texture_default(tex);
  tex->type = node->type - TEX_NODE_PROC;

  if (tex->type == TEX_WOOD) {
    tex->stype = TEX_BANDNOISE;
  }
}

/* Node type definitions */
#define TexDef(TEXTYPE, outputs, name, Name) \
  void register_node_type_tex_proc_##name(void) \
  { \
    static bNodeType ntype; \
\
    tex_node_type_base(&ntype, TEX_NODE_PROC + TEXTYPE, Name, NODE_CLASS_TEXTURE); \
    blender::bke::node_type_socket_templates(&ntype, name##_inputs, outputs); \
    blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE); \
    ntype.initfunc = init; \
    node_type_storage(&ntype, "Tex", node_free_standard_storage, node_copy_standard_storage); \
    ntype.exec_fn = name##_exec; \
    ntype.flag |= NODE_PREVIEW; \
\
    nodeRegisterType(&ntype); \
  }

#define C outputs_color_only
#define CV outputs_both

TexDef(TEX_VORONOI, CV, voronoi, "Voronoi") TexDef(TEX_BLEND, C, blend, "Blend");
TexDef(TEX_MAGIC, C, magic, "Magic") TexDef(TEX_MARBLE, CV, marble, "Marble");
TexDef(TEX_CLOUDS, CV, clouds, "Clouds") TexDef(TEX_WOOD, CV, wood, "Wood");
TexDef(TEX_MUSGRAVE, CV, musgrave, "Musgrave") TexDef(TEX_NOISE, C, noise, "Noise");
TexDef(TEX_STUCCI, CV, stucci, "Stucci");
TexDef(TEX_DISTNOISE, CV, distnoise, "Distorted Noise");
