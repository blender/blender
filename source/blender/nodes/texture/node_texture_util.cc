/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

/*
 * HOW TEXTURE NODES WORK
 *
 * In contrast to Shader nodes, which place a color into the output
 * stack when executed, Texture nodes place a TexDelegate* there. To
 * obtain a color value from this, a node further up the chain reads
 * the TexDelegate* from its input stack, and uses tex_call_delegate to
 * retrieve the color from the delegate.
 *
 * comments: (ton)
 *
 * This system needs recode, a node system should rely on the stack, and
 * callbacks for nodes only should evaluate their own node, not recursively go
 * over other previous ones.
 */

#include "BKE_node_runtime.hh"

#include "NOD_texture.h"

#include "node_texture_util.hh"
#include "node_util.hh"

bool tex_node_poll_default(const blender::bke::bNodeType * /*ntype*/,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "TextureNodeTree")) {
    *r_disabled_hint = RPT_("Not a texture node tree");
    return false;
  }
  return true;
}

void tex_node_type_base(blender::bke::bNodeType *ntype, int type, const char *name, short nclass)
{
  blender::bke::node_type_base(ntype, type, name, nclass);

  ntype->poll = tex_node_poll_default;
  ntype->insert_link = node_insert_link_default;
}

static void tex_call_delegate(TexDelegate *dg, float *out, TexParams *params, short thread)
{
  if (dg->node->runtime->need_exec) {
    dg->fn(out, params, dg->node, dg->in, thread);
  }
}

static void tex_input(float *out, int num, bNodeStack *in, TexParams *params, short thread)
{
  TexDelegate *dg = static_cast<TexDelegate *>(in->data);
  if (dg) {
    tex_call_delegate(dg, in->vec, params, thread);

    if (in->hasoutput && in->sockettype == SOCK_FLOAT) {
      in->vec[1] = in->vec[2] = in->vec[0];
    }
  }
  memcpy(out, in->vec, num * sizeof(float));
}

void tex_input_vec(float *out, bNodeStack *in, TexParams *params, short thread)
{
  tex_input(out, 3, in, params, thread);
}

void tex_input_rgba(float *out, bNodeStack *in, TexParams *params, short thread)
{
  tex_input(out, 4, in, params, thread);

  if (in->hasoutput && in->sockettype == SOCK_FLOAT) {
    out[1] = out[2] = out[0];
    out[3] = 1;
  }

  if (in->hasoutput && in->sockettype == SOCK_VECTOR) {
    out[0] = out[0] * 0.5f + 0.5f;
    out[1] = out[1] * 0.5f + 0.5f;
    out[2] = out[2] * 0.5f + 0.5f;
    out[3] = 1;
  }
}

float tex_input_value(bNodeStack *in, TexParams *params, short thread)
{
  float out[4];
  tex_input_vec(out, in, params, thread);
  return out[0];
}

void params_from_cdata(TexParams *out, TexCallData *in)
{
  out->co = in->co;
  out->dxt = in->dxt;
  out->dyt = in->dyt;
  out->previewco = in->co;
  out->osatex = in->osatex;
  out->cfra = in->cfra;
  out->mtex = in->mtex;
}

void tex_output(bNode *node,
                bNodeExecData *execdata,
                bNodeStack **in,
                bNodeStack *out,
                TexFn texfn,
                TexCallData *cdata)
{
  TexDelegate *dg;

  if (node->flag & NODE_MUTED) {
    /* do not add a delegate if the node is muted */
    return;
  }

  if (!out->data) {
    /* Freed in tex_end_exec (node.cc) */
    dg = MEM_new<TexDelegate>("tex delegate");
    out->data = dg;
  }
  else {
    dg = static_cast<TexDelegate *>(out->data);
  }

  dg->cdata = cdata;
  dg->fn = texfn;
  dg->node = node;
  dg->preview = execdata->preview;
  memcpy(dg->in, in, MAX_SOCKET * sizeof(bNodeStack *));
  dg->type = out->sockettype;
}

void ntreeTexCheckCyclics(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {

    if (node->type == TEX_NODE_TEXTURE && node->id) {
      /* custom2 stops the node from rendering */
      if (node->custom1) {
        node->custom2 = 1;
        node->custom1 = 0;
      }
      else {
        Tex *tex = (Tex *)node->id;

        node->custom2 = 0;

        node->custom1 = 1;
        if (tex->use_nodes && tex->nodetree) {
          ntreeTexCheckCyclics(tex->nodetree);
        }
        node->custom1 = 0;
      }
    }
  }
}
