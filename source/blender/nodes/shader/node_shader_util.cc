/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_node_runtime.hh"

#include "node_shader_util.hh"

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

#include "RE_engine.h"

#include "node_exec.hh"

bool sh_node_poll_default(const bNodeType * /*ntype*/,
                          const bNodeTree *ntree,
                          const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "ShaderNodeTree")) {
    *r_disabled_hint = TIP_("Not a shader node tree");
    return false;
  }
  return true;
}

static bool sh_fn_poll_default(const bNodeType * /*ntype*/,
                               const bNodeTree *ntree,
                               const char **r_disabled_hint)
{
  if (!STR_ELEM(ntree->idname, "ShaderNodeTree", "GeometryNodeTree")) {
    *r_disabled_hint = TIP_("Not a shader or geometry node tree");
    return false;
  }
  return true;
}

void sh_node_type_base(bNodeType *ntype, int type, const char *name, short nclass)
{
  blender::bke::node_type_base(ntype, type, name, nclass);

  ntype->poll = sh_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
  ntype->gather_add_node_search_ops = blender::nodes::search_node_add_ops_for_basic_node;
}

void sh_fn_node_type_base(bNodeType *ntype, int type, const char *name, short nclass)
{
  sh_node_type_base(ntype, type, name, nclass);
  ntype->poll = sh_fn_poll_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
  ntype->gather_add_node_search_ops = blender::nodes::search_node_add_ops_for_basic_node;
}

bool line_style_shader_nodes_poll(const bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return snode->shaderfrom == SNODE_SHADER_LINESTYLE;
}

bool world_shader_nodes_poll(const bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return snode->shaderfrom == SNODE_SHADER_WORLD;
}

bool object_shader_nodes_poll(const bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return snode->shaderfrom == SNODE_SHADER_OBJECT;
}

bool object_cycles_shader_nodes_poll(const bContext *C)
{
  if (!object_shader_nodes_poll(C)) {
    return false;
  }
  const RenderEngineType *engine_type = CTX_data_engine_type(C);
  return STREQ(engine_type->idname, "CYCLES");
}

bool object_eevee_shader_nodes_poll(const bContext *C)
{
  if (!object_shader_nodes_poll(C)) {
    return false;
  }
  const RenderEngineType *engine_type = CTX_data_engine_type(C);
  return STREQ(engine_type->idname, "BLENDER_EEVEE");
}

/* ****** */

static void nodestack_get_vec(float *in, short type_in, bNodeStack *ns)
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

void node_gpu_stack_from_data(GPUNodeStack *gs, int type, bNodeStack *ns)
{
  memset(gs, 0, sizeof(*gs));

  if (ns == nullptr) {
    /* node_get_stack() will generate nullptr bNodeStack pointers
     * for unknown/unsupported types of sockets. */
    zero_v4(gs->vec);
    gs->link = nullptr;
    gs->type = GPU_NONE;
    gs->hasinput = false;
    gs->hasoutput = false;
    gs->sockettype = type;
  }
  else {
    nodestack_get_vec(gs->vec, type, ns);
    gs->link = (GPUNodeLink *)ns->data;

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
  if (ns) {
    copy_v4_v4(ns->vec, gs->vec);
    ns->data = gs->link;
    ns->sockettype = gs->sockettype;
  }
}

static void gpu_stack_from_data_list(GPUNodeStack *gs, ListBase *sockets, bNodeStack **ns)
{
  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, sockets, i) {
    node_gpu_stack_from_data(&gs[i], socket->type, ns[i]);
  }

  gs[i].end = true;
}

static void data_from_gpu_stack_list(ListBase *sockets, bNodeStack **ns, GPUNodeStack *gs)
{
  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, sockets, i) {
    node_data_from_gpu_stack(ns[i], &gs[i]);
  }
}

bool blender::bke::nodeSupportsActiveFlag(const bNode *node, int sub_activity)
{
  BLI_assert(ELEM(sub_activity, NODE_ACTIVE_TEXTURE, NODE_ACTIVE_PAINT_CANVAS));
  switch (sub_activity) {
    case NODE_ACTIVE_TEXTURE:
      return node->typeinfo->nclass == NODE_CLASS_TEXTURE;
    case NODE_ACTIVE_PAINT_CANVAS:
      return ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_ATTRIBUTE);
  }
  return false;
}

static bNode *node_get_active(bNodeTree *ntree, int sub_activity)
{
  BLI_assert(ELEM(sub_activity, NODE_ACTIVE_TEXTURE, NODE_ACTIVE_PAINT_CANVAS));
  /* this is the node we texture paint and draw in textured draw */
  bNode *inactivenode = nullptr, *activetexnode = nullptr, *activegroup = nullptr;
  bool hasgroup = false;

  if (!ntree) {
    return nullptr;
  }

  for (bNode *node : ntree->all_nodes()) {
    if (node->flag & sub_activity) {
      activetexnode = node;
      /* if active we can return immediately */
      if (node->flag & NODE_ACTIVE) {
        return node;
      }
    }
    else if (!inactivenode && blender::bke::nodeSupportsActiveFlag(node, sub_activity)) {
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
    bNode *tnode = node_get_active((bNodeTree *)activegroup->id, sub_activity);
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
    for (bNode *node : ntree->all_nodes()) {
      if (node->type == NODE_GROUP) {
        bNode *tnode = node_get_active((bNodeTree *)node->id, sub_activity);
        if (tnode && ((tnode->flag & sub_activity) || !inactivenode)) {
          return tnode;
        }
      }
    }
  }

  return inactivenode;
}

bNode *nodeGetActiveTexture(bNodeTree *ntree)
{
  return node_get_active(ntree, NODE_ACTIVE_TEXTURE);
}

namespace blender::bke {

bNode *nodeGetActivePaintCanvas(bNodeTree *ntree)
{
  return node_get_active(ntree, NODE_ACTIVE_PAINT_CANVAS);
}
}  // namespace blender::bke

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
      if ((output_node != nullptr) && (node == output_node)) {
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

void node_shader_gpu_bump_tex_coord(GPUMaterial *mat, bNode * /*node*/, GPUNodeLink **link)
{
  GPU_link(mat, "differentiate_texco", *link, link);
}

void node_shader_gpu_default_tex_coord(GPUMaterial *mat, bNode *node, GPUNodeLink **link)
{
  if (!*link) {
    *link = GPU_attribute(mat, CD_ORCO, "");
    node_shader_gpu_bump_tex_coord(mat, node, link);
  }
}

void node_shader_gpu_tex_mapping(GPUMaterial *mat,
                                 bNode *node,
                                 GPUNodeStack *in,
                                 GPUNodeStack * /*out*/)
{
  NodeTexBase *base = (NodeTexBase *)node->storage;
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
  const float *xyz_to_rgb = IMB_colormanagement_get_xyz_to_scene_linear();
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
