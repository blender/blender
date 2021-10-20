/*
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
 */

/** \file
 * \ingroup nodes
 */

#include "DNA_node_types.h"

#include "node_shader_util.h"

#include "node_exec.h"

bool sh_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree, const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "ShaderNodeTree")) {
    *r_disabled_hint = "Not a shader node tree";
    return false;
  }
  return true;
}

static bool sh_fn_poll_default(bNodeType *UNUSED(ntype),
                               bNodeTree *ntree,
                               const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "ShaderNodeTree") && !STREQ(ntree->idname, "GeometryNodeTree")) {
    *r_disabled_hint = "Not a shader or geometry node tree";
    return false;
  }
  return true;
}

void sh_node_type_base(
    struct bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  node_type_base(ntype, type, name, nclass, flag);

  ntype->poll = sh_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->update_internal_links = node_update_internal_links_default;
}

void sh_fn_node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
  sh_node_type_base(ntype, type, name, nclass, flag);
  ntype->poll = sh_fn_poll_default;
}

/* ****** */

void nodestack_get_vec(float *in, short type_in, bNodeStack *ns)
{
  const float *from = ns->vec;

  if (type_in == SOCK_FLOAT) {
    if (ns->sockettype == SOCK_FLOAT) {
      *in = *from;
    }
    else {
      *in = (from[0] + from[1] + from[2]) / 3.0f;
    }
  }
  else if (type_in == SOCK_VECTOR) {
    if (ns->sockettype == SOCK_FLOAT) {
      in[0] = from[0];
      in[1] = from[0];
      in[2] = from[0];
    }
    else {
      copy_v3_v3(in, from);
    }
  }
  else { /* type_in==SOCK_RGBA */
    if (ns->sockettype == SOCK_RGBA) {
      copy_v4_v4(in, from);
    }
    else if (ns->sockettype == SOCK_FLOAT) {
      in[0] = from[0];
      in[1] = from[0];
      in[2] = from[0];
      in[3] = 1.0f;
    }
    else {
      copy_v3_v3(in, from);
      in[3] = 1.0f;
    }
  }
}

void node_gpu_stack_from_data(struct GPUNodeStack *gs, int type, bNodeStack *ns)
{
  memset(gs, 0, sizeof(*gs));

  if (ns == NULL) {
    /* node_get_stack() will generate NULL bNodeStack pointers
     * for unknown/unsupported types of sockets. */
    zero_v4(gs->vec);
    gs->link = NULL;
    gs->type = GPU_NONE;
    gs->hasinput = false;
    gs->hasoutput = false;
    gs->sockettype = type;
  }
  else {
    nodestack_get_vec(gs->vec, type, ns);
    gs->link = ns->data;

    if (type == SOCK_FLOAT) {
      gs->type = GPU_FLOAT;
    }
    else if (type == SOCK_INT) {
      gs->type = GPU_FLOAT; /* HACK: Support as float. */
    }
    else if (type == SOCK_VECTOR) {
      gs->type = GPU_VEC3;
    }
    else if (type == SOCK_RGBA) {
      gs->type = GPU_VEC4;
    }
    else if (type == SOCK_SHADER) {
      gs->type = GPU_CLOSURE;
    }
    else {
      gs->type = GPU_NONE;
    }

    gs->hasinput = ns->hasinput && ns->data;
    /* XXX Commented out the ns->data check here, as it seems it's not always set,
     *     even though there *is* a valid connection/output... But that might need
     *     further investigation.
     */
    gs->hasoutput = ns->hasoutput /*&& ns->data*/;
    gs->sockettype = ns->sockettype;
  }
}

void node_data_from_gpu_stack(bNodeStack *ns, GPUNodeStack *gs)
{
  copy_v4_v4(ns->vec, gs->vec);
  ns->data = gs->link;
  ns->sockettype = gs->sockettype;
}

static void gpu_stack_from_data_list(GPUNodeStack *gs, ListBase *sockets, bNodeStack **ns)
{
  bNodeSocket *sock;
  int i;

  for (sock = sockets->first, i = 0; sock; sock = sock->next, i++) {
    node_gpu_stack_from_data(&gs[i], sock->type, ns[i]);
  }

  gs[i].end = true;
}

static void data_from_gpu_stack_list(ListBase *sockets, bNodeStack **ns, GPUNodeStack *gs)
{
  bNodeSocket *sock;
  int i;

  for (sock = sockets->first, i = 0; sock; sock = sock->next, i++) {
    node_data_from_gpu_stack(ns[i], &gs[i]);
  }
}

bNode *nodeGetActiveTexture(bNodeTree *ntree)
{
  /* this is the node we texture paint and draw in textured draw */
  bNode *node, *tnode, *inactivenode = NULL, *activetexnode = NULL, *activegroup = NULL;
  bool hasgroup = false;

  if (!ntree) {
    return NULL;
  }

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->flag & NODE_ACTIVE_TEXTURE) {
      activetexnode = node;
      /* if active we can return immediately */
      if (node->flag & NODE_ACTIVE) {
        return node;
      }
    }
    else if (!inactivenode && node->typeinfo->nclass == NODE_CLASS_TEXTURE) {
      inactivenode = node;
    }
    else if (node->type == NODE_GROUP) {
      if (node->flag & NODE_ACTIVE) {
        activegroup = node;
      }
      else {
        hasgroup = true;
      }
    }
  }

  /* first, check active group for textures */
  if (activegroup) {
    tnode = nodeGetActiveTexture((bNodeTree *)activegroup->id);
    /* active node takes priority, so ignore any other possible nodes here */
    if (tnode) {
      return tnode;
    }
  }

  if (activetexnode) {
    return activetexnode;
  }

  if (hasgroup) {
    /* node active texture node in this tree, look inside groups */
    for (node = ntree->nodes.first; node; node = node->next) {
      if (node->type == NODE_GROUP) {
        tnode = nodeGetActiveTexture((bNodeTree *)node->id);
        if (tnode && ((tnode->flag & NODE_ACTIVE_TEXTURE) || !inactivenode)) {
          return tnode;
        }
      }
    }
  }

  return inactivenode;
}

void ntreeExecGPUNodes(bNodeTreeExec *exec, GPUMaterial *mat, bNode *output_node)
{
  bNodeExec *nodeexec;
  bNode *node;
  int n;
  bNodeStack *stack;
  bNodeStack *nsin[MAX_SOCKET];  /* arbitrary... watch this */
  bNodeStack *nsout[MAX_SOCKET]; /* arbitrary... watch this */
  GPUNodeStack gpuin[MAX_SOCKET + 1], gpuout[MAX_SOCKET + 1];
  bool do_it;

  stack = exec->stack;

  for (n = 0, nodeexec = exec->nodeexec; n < exec->totnodes; n++, nodeexec++) {
    node = nodeexec->node;

    do_it = false;
    /* for groups, only execute outputs for edited group */
    if (node->typeinfo->nclass == NODE_CLASS_OUTPUT) {
      if ((output_node != NULL) && (node == output_node)) {
        do_it = true;
      }
    }
    else {
      do_it = true;
    }

    if (do_it) {
      if (node->typeinfo->gpu_fn) {
        node_get_stack(node, stack, nsin, nsout);
        gpu_stack_from_data_list(gpuin, &node->inputs, nsin);
        gpu_stack_from_data_list(gpuout, &node->outputs, nsout);
        if (node->typeinfo->gpu_fn(mat, node, &nodeexec->data, gpuin, gpuout)) {
          data_from_gpu_stack_list(&node->outputs, nsout, gpuout);
        }
      }
    }
  }
}

void node_shader_gpu_bump_tex_coord(GPUMaterial *mat, bNode *node, GPUNodeLink **link)
{
  if (node->branch_tag == 1) {
    /* Add one time the value fo derivative to the input vector. */
    GPU_link(mat, "dfdx_v3", *link, link);
  }
  else if (node->branch_tag == 2) {
    /* Add one time the value fo derivative to the input vector. */
    GPU_link(mat, "dfdy_v3", *link, link);
  }
  else {
    /* nothing to do, reference center value. */
  }
}

void node_shader_gpu_default_tex_coord(GPUMaterial *mat, bNode *node, GPUNodeLink **link)
{
  if (!*link) {
    *link = GPU_attribute(mat, CD_ORCO, "");
    GPU_link(mat, "generated_texco", GPU_builtin(GPU_VIEW_POSITION), *link, link);
    node_shader_gpu_bump_tex_coord(mat, node, link);
  }
}

void node_shader_gpu_tex_mapping(GPUMaterial *mat,
                                 bNode *node,
                                 GPUNodeStack *in,
                                 GPUNodeStack *UNUSED(out))
{
  NodeTexBase *base = node->storage;
  TexMapping *texmap = &base->tex_mapping;
  float domin = (texmap->flag & TEXMAP_CLIP_MIN) != 0;
  float domax = (texmap->flag & TEXMAP_CLIP_MAX) != 0;

  if (domin || domax || !(texmap->flag & TEXMAP_UNIT_MATRIX)) {
    static float max[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    static float min[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    GPUNodeLink *tmin, *tmax, *tmat0, *tmat1, *tmat2, *tmat3;

    tmin = GPU_uniform((domin) ? texmap->min : min);
    tmax = GPU_uniform((domax) ? texmap->max : max);
    tmat0 = GPU_uniform((float *)texmap->mat[0]);
    tmat1 = GPU_uniform((float *)texmap->mat[1]);
    tmat2 = GPU_uniform((float *)texmap->mat[2]);
    tmat3 = GPU_uniform((float *)texmap->mat[3]);

    GPU_link(mat, "mapping_mat4", in[0].link, tmat0, tmat1, tmat2, tmat3, tmin, tmax, &in[0].link);

    if (texmap->type == TEXMAP_TYPE_NORMAL) {
      GPU_link(mat, "vector_normalize", in[0].link, &in[0].link);
    }
  }
}

void get_XYZ_to_RGB_for_gpu(XYZ_to_RGB *data)
{
  const float *xyz_to_rgb = IMB_colormanagement_get_xyz_to_rgb();
  data->r[0] = xyz_to_rgb[0];
  data->r[1] = xyz_to_rgb[3];
  data->r[2] = xyz_to_rgb[6];
  data->g[0] = xyz_to_rgb[1];
  data->g[1] = xyz_to_rgb[4];
  data->g[2] = xyz_to_rgb[7];
  data->b[0] = xyz_to_rgb[2];
  data->b[1] = xyz_to_rgb[5];
  data->b[2] = xyz_to_rgb[8];
}
