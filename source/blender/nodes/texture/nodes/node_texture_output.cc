/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup texnodes
 */

#include "BLI_string.h"

#include "NOD_texture.h"
#include "node_texture_util.hh"

/* **************** COMPOSITE ******************** */
static bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};

/* applies to render pipeline */
static void exec(void *data,
                 int /*thread*/,
                 bNode *node,
                 bNodeExecData * /*execdata*/,
                 bNodeStack **in,
                 bNodeStack ** /*out*/)
{
  TexCallData *cdata = (TexCallData *)data;
  TexResult *target = cdata->target;

  if (cdata->do_preview) {
    TexParams params;
    params_from_cdata(&params, cdata);

    tex_input_rgba(target->trgba, in[0], &params, cdata->thread);
  }
  else {
    /* 0 means don't care, so just use first */
    if (cdata->which_output == node->custom1 || (cdata->which_output == 0 && node->custom1 == 1)) {
      TexParams params;
      params_from_cdata(&params, cdata);

      tex_input_rgba(target->trgba, in[0], &params, cdata->thread);

      target->tin = (target->trgba[0] + target->trgba[1] + target->trgba[2]) / 3.0f;
      target->talpha = true;
    }
  }
}

static void unique_name(bNode *node)
{
  TexNodeOutput *tno = (TexNodeOutput *)node->storage;
  char new_name[sizeof(tno->name)];
  int new_len = 0;
  int suffix;
  bNode *i;
  const char *name = tno->name;

  new_name[0] = '\0';
  i = node;
  while (i->prev) {
    i = i->prev;
  }
  for (; i; i = i->next) {
    if (i == node || i->type != TEX_NODE_OUTPUT ||
        !STREQ(name, ((TexNodeOutput *)(i->storage))->name)) {
      continue;
    }

    if (new_name[0] == '\0') {
      int len = strlen(name);
      if (len >= 4 && sscanf(name + len - 4, ".%03d", &suffix) == 1) {
        new_len = len;
      }
      else {
        suffix = 0;
        new_len = len + 4;
        if (new_len > (sizeof(tno->name) - 1)) {
          new_len = (sizeof(tno->name) - 1);
        }
      }

      BLI_strncpy(new_name, name, sizeof(tno->name));
      name = new_name;
    }
    BLI_sprintf(new_name + new_len - 4, ".%03d", ++suffix);
  }

  if (new_name[0] != '\0') {
    BLI_strncpy(tno->name, new_name, sizeof(tno->name));
  }
}

static void assign_index(struct bNode *node)
{
  bNode *tnode;
  int index = 1;

  tnode = node;
  while (tnode->prev) {
    tnode = tnode->prev;
  }

check_index:
  for (; tnode; tnode = tnode->next) {
    if (tnode->type == TEX_NODE_OUTPUT && tnode != node) {
      if (tnode->custom1 == index) {
        index++;
        goto check_index;
      }
    }
  }

  node->custom1 = index;
}

static void init(bNodeTree * /*ntree*/, bNode *node)
{
  TexNodeOutput *tno = MEM_cnew<TexNodeOutput>("TEX_output");
  node->storage = tno;

  strcpy(tno->name, "Default");
  unique_name(node);
  assign_index(node);
}

static void copy(bNodeTree *dest_ntree, bNode *dest_node, const bNode *src_node)
{
  node_copy_standard_storage(dest_ntree, dest_node, src_node);
  unique_name(dest_node);
  assign_index(dest_node);
}

void register_node_type_tex_output(void)
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_OUTPUT, "Output", NODE_CLASS_OUTPUT);
  node_type_socket_templates(&ntype, inputs, nullptr);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.initfunc = init;
  node_type_storage(&ntype, "TexNodeOutput", node_free_standard_storage, copy);
  ntype.exec_fn = exec;

  ntype.flag |= NODE_PREVIEW;
  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}
