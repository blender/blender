/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <cstring>

#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_linestyle.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"
#include "BKE_texture.h"

#include "NOD_texture.h"
#include "node_common.h"
#include "node_exec.hh"
#include "node_texture_util.hh"
#include "node_util.hh"

#include "RNA_prototypes.hh"

#include "UI_resources.hh"

static void texture_get_from_context(const bContext *C,
                                     blender::bke::bNodeTreeType * /*treetype*/,
                                     bNodeTree **r_ntree,
                                     ID **r_id,
                                     ID **r_from)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Tex *tx = nullptr;

  if (snode->texfrom == SNODE_TEX_BRUSH) {
    Brush *brush = nullptr;

    if (ob && (ob->mode & OB_MODE_SCULPT)) {
      brush = BKE_paint_brush(&scene->toolsettings->sculpt->paint);
    }
    else {
      brush = BKE_paint_brush(&scene->toolsettings->imapaint.paint);
    }

    if (brush) {
      *r_from = (ID *)brush;
      tx = give_current_brush_texture(brush);
      if (tx) {
        *r_id = &tx->id;
        *r_ntree = tx->nodetree;
      }
    }
  }
  else if (snode->texfrom == SNODE_TEX_LINESTYLE) {
    FreestyleLineStyle *linestyle = BKE_linestyle_active_from_view_layer(view_layer);
    if (linestyle) {
      *r_from = (ID *)linestyle;
      tx = give_current_linestyle_texture(linestyle);
      if (tx) {
        *r_id = &tx->id;
        *r_ntree = tx->nodetree;
      }
    }
  }
}

static void foreach_nodeclass(void *calldata, blender::bke::bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_PATTERN, N_("Patterns"));
  func(calldata, NODE_CLASS_TEXTURE, N_("Textures"));
  func(calldata, NODE_CLASS_CONVERTER, N_("Converter"));
  func(calldata, NODE_CLASS_DISTORT, N_("Distort"));
  func(calldata, NODE_CLASS_GROUP, N_("Group"));
  func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

/* XXX muting disabled in previews because of threading issues with the main execution
 * it works here, but disabled for consistency
 */
#if 1
static void localize(bNodeTree *localtree, bNodeTree * /*ntree*/)
{
  bNode *node, *node_next;

  /* replace muted nodes and reroute nodes by internal links */
  for (node = static_cast<bNode *>(localtree->nodes.first); node; node = node_next) {
    node_next = node->next;

    if (node->is_muted() || node->is_reroute()) {
      blender::bke::node_internal_relink(*localtree, *node);
      blender::bke::node_tree_free_local_node(*localtree, *node);
    }
  }
}
#else
static void localize(bNodeTree * /*localtree*/, bNodeTree * /*ntree*/) {}
#endif

static void update(bNodeTree *ntree)
{
  ntree_update_reroute_nodes(ntree);
}

static bool texture_node_tree_socket_type_valid(blender::bke::bNodeTreeType * /*ntreetype*/,
                                                blender::bke::bNodeSocketType *socket_type)
{
  return blender::bke::node_is_static_socket_type(*socket_type) &&
         ELEM(socket_type->type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA);
}

blender::bke::bNodeTreeType *ntreeType_Texture;

void register_node_tree_type_tex()
{
  blender::bke::bNodeTreeType *tt = ntreeType_Texture = MEM_new<blender::bke::bNodeTreeType>(
      __func__);

  tt->type = NTREE_TEXTURE;
  tt->idname = "TextureNodeTree";
  tt->group_idname = "TextureNodeGroup";
  tt->ui_name = N_("Texture Node Editor");
  tt->ui_icon = ICON_NODE_TEXTURE; /* Defined in `drawnode.cc`. */
  tt->ui_description = N_("Edit textures using nodes");

  tt->foreach_nodeclass = foreach_nodeclass;
  tt->update = update;
  tt->localize = localize;
  tt->get_from_context = texture_get_from_context;
  tt->valid_socket_type = texture_node_tree_socket_type_valid;

  tt->rna_ext.srna = &RNA_TextureNodeTree;

  blender::bke::node_tree_type_add(*tt);
}

/**** Material/Texture trees ****/

bNodeThreadStack *ntreeGetThreadStack(bNodeTreeExec *exec, int thread)
{
  ListBase *lb = &exec->threadstack[thread];
  bNodeThreadStack *nts;

  for (nts = (bNodeThreadStack *)lb->first; nts; nts = nts->next) {
    if (!nts->used) {
      nts->used = true;
      break;
    }
  }

  if (!nts) {
    nts = MEM_callocN<bNodeThreadStack>("bNodeThreadStack");
    nts->stack = (bNodeStack *)MEM_dupallocN(exec->stack);
    nts->used = true;
    BLI_addtail(lb, nts);
  }

  return nts;
}

void ntreeReleaseThreadStack(bNodeThreadStack *nts)
{
  nts->used = false;
}

bool ntreeExecThreadNodes(bNodeTreeExec *exec, bNodeThreadStack *nts, void *callerdata, int thread)
{
  bNodeStack *nsin[MAX_SOCKET] = {nullptr};  /* arbitrary... watch this */
  bNodeStack *nsout[MAX_SOCKET] = {nullptr}; /* arbitrary... watch this */
  bNodeExec *nodeexec;
  bNode *node;
  int n;

  /* nodes are presorted, so exec is in order of list */

  for (n = 0, nodeexec = exec->nodeexec; n < exec->totnodes; n++, nodeexec++) {
    node = nodeexec->node;
    if (node->runtime->need_exec) {
      node_get_stack(node, nts->stack, nsin, nsout);
      /* Handle muted nodes...
       * If the mute func is not set, assume the node should never be muted,
       * and hence execute it!
       */
      if (node->typeinfo->exec_fn && !node->is_muted()) {
        node->typeinfo->exec_fn(callerdata, thread, node, &nodeexec->data, nsin, nsout);
      }
    }
  }

  /* signal to that all went OK, for render */
  return true;
}

bNodeTreeExec *ntreeTexBeginExecTree_internal(bNodeExecContext *context,
                                              bNodeTree *ntree,
                                              bNodeInstanceKey parent_key)
{
  bNodeTreeExec *exec;

  /* common base initialization */
  exec = ntree_exec_begin(context, ntree, parent_key);

  /* allocate the thread stack listbase array */
  exec->threadstack = MEM_calloc_arrayN<ListBase>(BLENDER_MAX_THREADS, "thread stack array");

  LISTBASE_FOREACH (bNode *, node, &exec->nodetree->nodes) {
    node->runtime->need_exec = 1;
  }

  return exec;
}

bNodeTreeExec *ntreeTexBeginExecTree(bNodeTree *ntree)
{
  bNodeExecContext context;
  bNodeTreeExec *exec;

  /* XXX hack: prevent exec data from being generated twice.
   * this should be handled by the renderer!
   */
  if (ntree->runtime->execdata) {
    return ntree->runtime->execdata;
  }

  exec = ntreeTexBeginExecTree_internal(&context, ntree, blender::bke::NODE_INSTANCE_KEY_BASE);

  /* XXX this should not be necessary, but is still used for compositor/shading/texture nodes,
   * which only store the ntree pointer. Should be fixed at some point!
   */
  ntree->runtime->execdata = exec;

  return exec;
}

/* free texture delegates */
static void tex_free_delegates(bNodeTreeExec *exec)
{
  bNodeStack *ns;
  int th, a;

  for (th = 0; th < BLENDER_MAX_THREADS; th++) {
    LISTBASE_FOREACH (bNodeThreadStack *, nts, &exec->threadstack[th]) {
      for (ns = nts->stack, a = 0; a < exec->stacksize; a++, ns++) {
        if (ns->data && !ns->is_copy) {
          MEM_freeN(ns->data);
        }
      }
    }
  }
}

void ntreeTexEndExecTree_internal(bNodeTreeExec *exec)
{
  int a;

  if (exec->threadstack) {
    tex_free_delegates(exec);

    for (a = 0; a < BLENDER_MAX_THREADS; a++) {
      LISTBASE_FOREACH (bNodeThreadStack *, nts, &exec->threadstack[a]) {
        if (nts->stack) {
          MEM_freeN(nts->stack);
        }
      }
      BLI_freelistN(&exec->threadstack[a]);
    }

    MEM_freeN(exec->threadstack);
    exec->threadstack = nullptr;
  }

  ntree_exec_end(exec);
}

void ntreeTexEndExecTree(bNodeTreeExec *exec)
{
  if (exec) {
    /* exec may get freed, so assign ntree */
    bNodeTree *ntree = exec->nodetree;
    ntreeTexEndExecTree_internal(exec);

    /* XXX: clear node-tree back-pointer to exec data,
     * same problem as noted in #ntreeBeginExecTree. */
    ntree->runtime->execdata = nullptr;
  }
}

int ntreeTexExecTree(bNodeTree *ntree,
                     TexResult *target,
                     const float co[3],
                     const short thread,
                     const Tex * /*tex*/,
                     short which_output,
                     int cfra,
                     int preview,
                     MTex *mtex)
{
  TexCallData data;
  int retval = TEX_INT;
  bNodeThreadStack *nts = nullptr;
  bNodeTreeExec *exec = ntree->runtime->execdata;

  data.co = co;
  data.target = target;
  data.do_preview = preview;
  data.do_manage = true;
  data.thread = thread;
  data.which_output = which_output;
  data.cfra = cfra;
  data.mtex = mtex;

  /* ensure execdata is only initialized once */
  if (!exec) {
    BLI_thread_lock(LOCK_NODES);
    if (!ntree->runtime->execdata) {
      ntreeTexBeginExecTree(ntree);
    }
    BLI_thread_unlock(LOCK_NODES);

    exec = ntree->runtime->execdata;
  }

  nts = ntreeGetThreadStack(exec, thread);
  ntreeExecThreadNodes(exec, nts, &data, thread);
  ntreeReleaseThreadStack(nts);

  retval |= TEX_RGB;

  return retval;
}
