/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "node_texture_util.hh"

namespace blender {

static bke::bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Color"), 1.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};

static void exec(void *data,
                 int /*thread*/,
                 bNode * /*node*/,
                 bNodeExecData * /*execdata*/,
                 bNodeStack **in,
                 bNodeStack ** /*out*/)
{
  TexCallData *cdata = static_cast<TexCallData *>(data);

  if (cdata->do_preview) {
    TexParams params;
    float col[4];
    params_from_cdata(&params, cdata);

    tex_input_rgba(col, in[0], &params, cdata->thread);
  }
}

void register_node_type_tex_viewer()
{
  static bke::bNodeType ntype;

  tex_node_type_base(&ntype, "TextureNodeViewer", TEX_NODE_VIEWER);
  ntype.ui_name = "Viewer";
  ntype.enum_name_legacy = "VIEWER";
  ntype.nclass = NODE_CLASS_OUTPUT;
  bke::node_type_socket_templates(&ntype, inputs, nullptr);
  ntype.exec_fn = exec;

  ntype.no_muting = true;
  ntype.flag |= NODE_PREVIEW;

  bke::node_register_type(ntype);
}

}  // namespace blender
