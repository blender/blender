/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spnode
 * \brief higher level node drawing for the node editor.
 */

#include <iomanip>

#include "MEM_guardedalloc.h"

#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_array.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BLT_translation.h"

#include "BKE_compute_contexts.hh"
#include "BKE_context.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"
#include "BKE_type_conversions.hh"

#include "DEG_depsgraph.h"

#include "BLF_api.h"

#include "BIF_glutil.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_viewport.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gpencil.h"
#include "ED_node.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_viewer_path.hh"

#include "UI_interface.hh"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_log.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "FN_field.hh"
#include "FN_field_cpp_type.hh"

#include "../interface/interface_intern.hh" /* TODO: Remove */

#include "node_intern.hh" /* own include */

namespace geo_log = blender::nodes::geo_eval_log;

using blender::GPointer;
using blender::Vector;

/**
 * This is passed to many functions which draw the node editor.
 */
struct TreeDrawContext {
  /**
   * Whether a viewer node is active in geometry nodes can not be determined by a flag on the node
   * alone. That's because if the node group with the viewer is used multiple times, it's only
   * active in one of these cases.
   * The active node is cached here to avoid doing the more expensive check for every viewer node
   * in the tree.
   */
  const bNode *active_geometry_nodes_viewer = nullptr;
  /**
   * Geometry nodes logs various data during execution. The logged data that corresponds to the
   * currently drawn node tree can be retrieved from the log below.
   */
  geo_log::GeoTreeLog *geo_tree_log = nullptr;
  /**
   * True if there is an active realtime compositor using the node tree, false otherwise.
   */
  bool used_by_realtime_compositor = false;
};

float ED_node_grid_size()
{
  return U.widget_unit;
}

void ED_node_tree_update(const bContext *C)
{
  using namespace blender::ed::space_node;

  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode) {
    snode_set_context(*C);

    id_us_ensure_real(&snode->nodetree->id);
  }
}

/* id is supposed to contain a node tree */
static bNodeTree *node_tree_from_ID(ID *id)
{
  if (id) {
    if (GS(id->name) == ID_NT) {
      return (bNodeTree *)id;
    }
    return ntreeFromID(id);
  }

  return nullptr;
}

void ED_node_tag_update_id(ID *id)
{
  bNodeTree *ntree = node_tree_from_ID(id);
  if (id == nullptr || ntree == nullptr) {
    return;
  }

  /* TODO(sergey): With the new dependency graph it should be just enough to only tag ntree itself.
   * All the users of this tree will have update flushed from the tree. */
  DEG_id_tag_update(&ntree->id, 0);

  if (ntree->type == NTREE_SHADER) {
    DEG_id_tag_update(id, 0);

    if (GS(id->name) == ID_MA) {
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, id);
    }
    else if (GS(id->name) == ID_LA) {
      WM_main_add_notifier(NC_LAMP | ND_LIGHTING, id);
    }
    else if (GS(id->name) == ID_WO) {
      WM_main_add_notifier(NC_WORLD | ND_WORLD, id);
    }
  }
  else if (ntree->type == NTREE_COMPOSIT) {
    WM_main_add_notifier(NC_SCENE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_TEXTURE) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_TEXTURE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_GEOMETRY) {
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, id);
  }
  else if (id == &ntree->id) {
    /* Node groups. */
    DEG_id_tag_update(id, 0);
  }
}

namespace blender::ed::space_node {

static void node_socket_add_tooltip_in_node_editor(TreeDrawContext * /*tree_draw_ctx*/,
                                                   const bNodeTree *ntree,
                                                   const bNode *node,
                                                   const bNodeSocket *sock,
                                                   uiLayout *layout);

/** Return true when \a a should be behind \a b and false otherwise. */
static bool compare_node_depth(const bNode *a, const bNode *b)
{
  /* These tell if either the node or any of the parent nodes is selected.
   * A selected parent means an unselected node is also in foreground! */
  bool a_select = (a->flag & NODE_SELECT) != 0, b_select = (b->flag & NODE_SELECT) != 0;
  bool a_active = (a->flag & NODE_ACTIVE) != 0, b_active = (b->flag & NODE_ACTIVE) != 0;

  /* If one is an ancestor of the other. */
  /* XXX there might be a better sorting algorithm for stable topological sort,
   * this is O(n^2) worst case. */
  for (bNode *parent = a->parent; parent; parent = parent->parent) {
    /* If B is an ancestor, it is always behind A. */
    if (parent == b) {
      return false;
    }
    /* Any selected ancestor moves the node forward. */
    if (parent->flag & NODE_ACTIVE) {
      a_active = true;
    }
    if (parent->flag & NODE_SELECT) {
      a_select = true;
    }
  }
  for (bNode *parent = b->parent; parent; parent = parent->parent) {
    /* If A is an ancestor, it is always behind B. */
    if (parent == a) {
      return true;
    }
    /* Any selected ancestor moves the node forward. */
    if (parent->flag & NODE_ACTIVE) {
      b_active = true;
    }
    if (parent->flag & NODE_SELECT) {
      b_select = true;
    }
  }

  /* One of the nodes is in the background and the other not. */
  if ((a->flag & NODE_BACKGROUND) && !(b->flag & NODE_BACKGROUND)) {
    return true;
  }
  if ((b->flag & NODE_BACKGROUND) && !(a->flag & NODE_BACKGROUND)) {
    return false;
  }

  /* One has a higher selection state (active > selected > nothing). */
  if (a_active && !b_active) {
    return false;
  }
  if (b_active && !a_active) {
    return true;
  }
  if (!b_select && (a_active || a_select)) {
    return false;
  }
  if (!a_select && (b_active || b_select)) {
    return true;
  }

  return false;
}

void node_sort(bNodeTree &ntree)
{
  Array<bNode *> sort_nodes = ntree.all_nodes();
  std::stable_sort(sort_nodes.begin(), sort_nodes.end(), compare_node_depth);

  /* If nothing was changed, exit early. Otherwise the node tree's runtime
   * node vector needs to be rebuilt, since it cannot be reordered in place. */
  if (sort_nodes == ntree.all_nodes()) {
    return;
  }

  BKE_ntree_update_tag_node_reordered(&ntree);

  ntree.runtime->nodes_by_id.clear();
  BLI_listbase_clear(&ntree.nodes);
  for (const int i : sort_nodes.index_range()) {
    BLI_addtail(&ntree.nodes, sort_nodes[i]);
    ntree.runtime->nodes_by_id.add_new(sort_nodes[i]);
    sort_nodes[i]->runtime->index_in_tree = i;
  }
}

static Array<uiBlock *> node_uiblocks_init(const bContext &C, Span<bNode *> nodes)
{
  Array<uiBlock *> blocks(nodes.size());
  /* Add node uiBlocks in drawing order - prevents events going to overlapping nodes. */
  for (const int i : nodes.index_range()) {
    const std::string block_name = "node_" + std::string(nodes[i]->name);
    blocks[i] = UI_block_begin(&C, CTX_wm_region(&C), block_name.c_str(), UI_EMBOSS);
    /* this cancels events for background nodes */
    UI_block_flag_enable(blocks[i], UI_BLOCK_CLIP_EVENTS);
  }

  return blocks;
}

float2 node_to_view(const bNode &node, const float2 &co)
{
  float2 result;
  nodeToView(&node, co.x, co.y, &result.x, &result.y);
  return result * UI_DPI_FAC;
}

void node_to_updated_rect(const bNode &node, rctf &r_rect)
{
  const float2 xmin_ymax = node_to_view(node, {node.offsetx, node.offsety});
  r_rect.xmin = xmin_ymax.x;
  r_rect.ymax = xmin_ymax.y;
  const float2 xmax_ymin = node_to_view(node,
                                        {node.offsetx + node.width, node.offsety - node.height});
  r_rect.xmax = xmax_ymin.x;
  r_rect.ymin = xmax_ymin.y;
}

float2 node_from_view(const bNode &node, const float2 &co)
{
  const float x = co.x / UI_DPI_FAC;
  const float y = co.y / UI_DPI_FAC;
  float2 result;
  nodeFromView(&node, x, y, &result.x, &result.y);
  return result;
}

/**
 * Based on settings and sockets in node, set drawing rect info.
 */
static void node_update_basis(const bContext &C,
                              TreeDrawContext &tree_draw_ctx,
                              bNodeTree &ntree,
                              bNode &node,
                              uiBlock &block)
{
  PointerRNA nodeptr;
  RNA_pointer_create(&ntree.id, &RNA_Node, &node, &nodeptr);

  const bool node_options = node.typeinfo->draw_buttons && (node.flag & NODE_OPTIONS);
  const bool inputs_first = node.inputs.first &&
                            !(node.outputs.first || (node.flag & NODE_PREVIEW) || node_options);

  /* Get "global" coordinates. */
  float2 loc = node_to_view(node, float2(0));
  /* Round the node origin because text contents are always pixel-aligned. */
  loc.x = round(loc.x);
  loc.y = round(loc.y);

  int dy = loc.y;

  /* Header. */
  dy -= NODE_DY;

  /* Add a little bit of padding above the top socket. */
  if (node.outputs.first || inputs_first) {
    dy -= NODE_DYS / 2;
  }

  /* Output sockets. */
  bool add_output_space = false;

  int buty;
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
    if (nodeSocketIsHidden(socket)) {
      continue;
    }

    PointerRNA sockptr;
    RNA_pointer_create(&ntree.id, &RNA_NodeSocket, socket, &sockptr);

    uiLayout *layout = UI_block_layout(&block,
                                       UI_LAYOUT_VERTICAL,
                                       UI_LAYOUT_PANEL,
                                       loc.x + NODE_DYS,
                                       dy,
                                       NODE_WIDTH(node) - NODE_DY,
                                       NODE_DY,
                                       0,
                                       UI_style_get_dpi());

    if (node.flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    /* Context pointers for current node and socket. */
    uiLayoutSetContextPointer(layout, "node", &nodeptr);
    uiLayoutSetContextPointer(layout, "socket", &sockptr);

    /* Align output buttons to the right. */
    uiLayout *row = uiLayoutRow(layout, true);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
    const char *socket_label = nodeSocketLabel(socket);
    socket->typeinfo->draw((bContext *)&C, row, &sockptr, &nodeptr, IFACE_(socket_label));

    node_socket_add_tooltip_in_node_editor(&tree_draw_ctx, &ntree, &node, socket, row);

    UI_block_align_end(&block);
    UI_block_layout_resolve(&block, nullptr, &buty);

    /* Ensure minimum socket height in case layout is empty. */
    buty = min_ii(buty, dy - NODE_DY);

    /* Round the socket location to stop it from jiggling. */
    socket->runtime->locx = round(loc.x + NODE_WIDTH(node));
    socket->runtime->locy = round(dy - NODE_DYS);

    dy = buty;
    if (socket->next) {
      dy -= NODE_SOCKDY;
    }

    add_output_space = true;
  }

  if (add_output_space) {
    dy -= NODE_DY / 4;
  }

  node.runtime->prvr.xmin = loc.x + NODE_DYS;
  node.runtime->prvr.xmax = loc.x + NODE_WIDTH(node) - NODE_DYS;

  /* preview rect? */
  if (node.flag & NODE_PREVIEW) {
    float aspect = 1.0f;

    if (node.runtime->preview_xsize && node.runtime->preview_ysize) {
      aspect = float(node.runtime->preview_ysize) / float(node.runtime->preview_xsize);
    }

    dy -= NODE_DYS / 2;
    node.runtime->prvr.ymax = dy;

    if (aspect <= 1.0f) {
      node.runtime->prvr.ymin = dy - aspect * (NODE_WIDTH(node) - NODE_DY);
    }
    else {
      /* Width correction of image. XXX huh? (ton) */
      float dx = (NODE_WIDTH(node) - NODE_DYS) - (NODE_WIDTH(node) - NODE_DYS) / aspect;

      node.runtime->prvr.ymin = dy - (NODE_WIDTH(node) - NODE_DY);

      node.runtime->prvr.xmin += 0.5f * dx;
      node.runtime->prvr.xmax -= 0.5f * dx;
    }

    dy = node.runtime->prvr.ymin - NODE_DYS / 2;

    /* Make sure that maximums are bigger or equal to minimums. */
    if (node.runtime->prvr.xmax < node.runtime->prvr.xmin) {
      SWAP(float, node.runtime->prvr.xmax, node.runtime->prvr.xmin);
    }
    if (node.runtime->prvr.ymax < node.runtime->prvr.ymin) {
      SWAP(float, node.runtime->prvr.ymax, node.runtime->prvr.ymin);
    }
  }

  /* Buttons rect? */
  if (node_options) {
    dy -= NODE_DYS / 2;

    uiLayout *layout = UI_block_layout(&block,
                                       UI_LAYOUT_VERTICAL,
                                       UI_LAYOUT_PANEL,
                                       loc.x + NODE_DYS,
                                       dy,
                                       NODE_WIDTH(node) - NODE_DY,
                                       0,
                                       0,
                                       UI_style_get_dpi());

    if (node.flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    uiLayoutSetContextPointer(layout, "node", &nodeptr);

    node.typeinfo->draw_buttons(layout, (bContext *)&C, &nodeptr);

    UI_block_align_end(&block);
    UI_block_layout_resolve(&block, nullptr, &buty);

    dy = buty - NODE_DYS / 2;
  }

  /* Input sockets. */
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (nodeSocketIsHidden(socket)) {
      continue;
    }

    PointerRNA sockptr;
    RNA_pointer_create(&ntree.id, &RNA_NodeSocket, socket, &sockptr);

    /* Add the half the height of a multi-input socket to cursor Y
     * to account for the increased height of the taller sockets. */
    float multi_input_socket_offset = 0.0f;
    if (socket->flag & SOCK_MULTI_INPUT) {
      if (socket->runtime->total_inputs > 2) {
        multi_input_socket_offset = (socket->runtime->total_inputs - 2) *
                                    NODE_MULTI_INPUT_LINK_GAP;
      }
    }
    dy -= multi_input_socket_offset * 0.5f;

    uiLayout *layout = UI_block_layout(&block,
                                       UI_LAYOUT_VERTICAL,
                                       UI_LAYOUT_PANEL,
                                       loc.x + NODE_DYS,
                                       dy,
                                       NODE_WIDTH(node) - NODE_DY,
                                       NODE_DY,
                                       0,
                                       UI_style_get_dpi());

    if (node.flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    /* Context pointers for current node and socket. */
    uiLayoutSetContextPointer(layout, "node", &nodeptr);
    uiLayoutSetContextPointer(layout, "socket", &sockptr);

    uiLayout *row = uiLayoutRow(layout, true);

    const char *socket_label = nodeSocketLabel(socket);
    socket->typeinfo->draw((bContext *)&C, row, &sockptr, &nodeptr, IFACE_(socket_label));

    node_socket_add_tooltip_in_node_editor(&tree_draw_ctx, &ntree, &node, socket, row);

    UI_block_align_end(&block);
    UI_block_layout_resolve(&block, nullptr, &buty);

    /* Ensure minimum socket height in case layout is empty. */
    buty = min_ii(buty, dy - NODE_DY);

    socket->runtime->locx = loc.x;
    /* Round the socket vertical position to stop it from jiggling. */
    socket->runtime->locy = round(dy - NODE_DYS);

    dy = buty - multi_input_socket_offset * 0.5;
    if (socket->next) {
      dy -= NODE_SOCKDY;
    }
  }

  /* Little bit of space in end. */
  if (node.inputs.first || (node.flag & (NODE_OPTIONS | NODE_PREVIEW)) == 0) {
    dy -= NODE_DYS / 2;
  }

  node.runtime->totr.xmin = loc.x;
  node.runtime->totr.xmax = loc.x + NODE_WIDTH(node);
  node.runtime->totr.ymax = loc.y;
  node.runtime->totr.ymin = min_ff(dy, loc.y - 2 * NODE_DY);

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side. */
  UI_block_bounds_set_explicit(&block,
                               node.runtime->totr.xmin - NODE_SOCKSIZE,
                               node.runtime->totr.ymin,
                               node.runtime->totr.xmax + NODE_SOCKSIZE,
                               node.runtime->totr.ymax);
}

/**
 * Based on settings in node, sets drawing rect info.
 */
static void node_update_hidden(bNode &node, uiBlock &block)
{
  int totin = 0, totout = 0;

  /* Get "global" coordinates. */
  float2 loc = node_to_view(node, float2(0));
  /* Round the node origin because text contents are always pixel-aligned. */
  loc.x = round(loc.x);
  loc.y = round(loc.y);

  /* Calculate minimal radius. */
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (!nodeSocketIsHidden(socket)) {
      totin++;
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
    if (!nodeSocketIsHidden(socket)) {
      totout++;
    }
  }

  float hiddenrad = HIDDEN_RAD;
  float tot = MAX2(totin, totout);
  if (tot > 4) {
    hiddenrad += 5.0f * float(tot - 4);
  }

  node.runtime->totr.xmin = loc.x;
  node.runtime->totr.xmax = loc.x + max_ff(NODE_WIDTH(node), 2 * hiddenrad);
  node.runtime->totr.ymax = loc.y + (hiddenrad - 0.5f * NODE_DY);
  node.runtime->totr.ymin = node.runtime->totr.ymax - 2 * hiddenrad;

  /* Output sockets. */
  float rad = float(M_PI) / (1.0f + float(totout));
  float drad = rad;

  LISTBASE_FOREACH (bNodeSocket *, socket, &node.outputs) {
    if (!nodeSocketIsHidden(socket)) {
      /* Round the socket location to stop it from jiggling. */
      socket->runtime->locx = round(node.runtime->totr.xmax - hiddenrad + sinf(rad) * hiddenrad);
      socket->runtime->locy = round(node.runtime->totr.ymin + hiddenrad + cosf(rad) * hiddenrad);
      rad += drad;
    }
  }

  /* Input sockets. */
  rad = drad = -float(M_PI) / (1.0f + float(totin));

  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (!nodeSocketIsHidden(socket)) {
      /* Round the socket location to stop it from jiggling. */
      socket->runtime->locx = round(node.runtime->totr.xmin + hiddenrad + sinf(rad) * hiddenrad);
      socket->runtime->locy = round(node.runtime->totr.ymin + hiddenrad + cosf(rad) * hiddenrad);
      rad += drad;
    }
  }

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side. */
  UI_block_bounds_set_explicit(&block,
                               node.runtime->totr.xmin - NODE_SOCKSIZE,
                               node.runtime->totr.ymin,
                               node.runtime->totr.xmax + NODE_SOCKSIZE,
                               node.runtime->totr.ymax);
}

static int node_get_colorid(TreeDrawContext &tree_draw_ctx, const bNode &node)
{
  const int nclass = (node.typeinfo->ui_class == nullptr) ? node.typeinfo->nclass :
                                                            node.typeinfo->ui_class(&node);
  switch (nclass) {
    case NODE_CLASS_INPUT:
      return TH_NODE_INPUT;
    case NODE_CLASS_OUTPUT: {
      if (node.type == GEO_NODE_VIEWER) {
        return &node == tree_draw_ctx.active_geometry_nodes_viewer ? TH_NODE_OUTPUT : TH_NODE;
      }
      return (node.flag & NODE_DO_OUTPUT) ? TH_NODE_OUTPUT : TH_NODE;
    }
    case NODE_CLASS_CONVERTER:
      return TH_NODE_CONVERTER;
    case NODE_CLASS_OP_COLOR:
      return TH_NODE_COLOR;
    case NODE_CLASS_OP_VECTOR:
      return TH_NODE_VECTOR;
    case NODE_CLASS_OP_FILTER:
      return TH_NODE_FILTER;
    case NODE_CLASS_GROUP:
      return TH_NODE_GROUP;
    case NODE_CLASS_INTERFACE:
      return TH_NODE_INTERFACE;
    case NODE_CLASS_MATTE:
      return TH_NODE_MATTE;
    case NODE_CLASS_DISTORT:
      return TH_NODE_DISTORT;
    case NODE_CLASS_TEXTURE:
      return TH_NODE_TEXTURE;
    case NODE_CLASS_SHADER:
      return TH_NODE_SHADER;
    case NODE_CLASS_SCRIPT:
      return TH_NODE_SCRIPT;
    case NODE_CLASS_PATTERN:
      return TH_NODE_PATTERN;
    case NODE_CLASS_LAYOUT:
      return TH_NODE_LAYOUT;
    case NODE_CLASS_GEOMETRY:
      return TH_NODE_GEOMETRY;
    case NODE_CLASS_ATTRIBUTE:
      return TH_NODE_ATTRIBUTE;
    default:
      return TH_NODE;
  }
}

static void node_draw_mute_line(const bContext &C,
                                const View2D &v2d,
                                const SpaceNode &snode,
                                const bNode &node)
{
  GPU_blend(GPU_BLEND_ALPHA);

  for (const bNodeLink *link : node.internal_links()) {
    if (!nodeLinkIsHidden(link)) {
      node_draw_link_bezier(C, v2d, snode, *link, TH_WIRE_INNER, TH_WIRE_INNER, TH_WIRE, false);
    }
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void node_socket_draw(const bNodeSocket &sock,
                             const float color[4],
                             const float color_outline[4],
                             float size,
                             int locx,
                             int locy,
                             uint pos_id,
                             uint col_id,
                             uint shape_id,
                             uint size_id,
                             uint outline_col_id)
{
  int flags;

  /* Set shape flags. */
  switch (sock.display_shape) {
    case SOCK_DISPLAY_SHAPE_DIAMOND:
    case SOCK_DISPLAY_SHAPE_DIAMOND_DOT:
      flags = GPU_KEYFRAME_SHAPE_DIAMOND;
      break;
    case SOCK_DISPLAY_SHAPE_SQUARE:
    case SOCK_DISPLAY_SHAPE_SQUARE_DOT:
      flags = GPU_KEYFRAME_SHAPE_SQUARE;
      break;
    default:
    case SOCK_DISPLAY_SHAPE_CIRCLE:
    case SOCK_DISPLAY_SHAPE_CIRCLE_DOT:
      flags = GPU_KEYFRAME_SHAPE_CIRCLE;
      break;
  }

  if (ELEM(sock.display_shape,
           SOCK_DISPLAY_SHAPE_DIAMOND_DOT,
           SOCK_DISPLAY_SHAPE_SQUARE_DOT,
           SOCK_DISPLAY_SHAPE_CIRCLE_DOT)) {
    flags |= GPU_KEYFRAME_SHAPE_INNER_DOT;
  }

  immAttr4fv(col_id, color);
  immAttr1u(shape_id, flags);
  immAttr1f(size_id, size);
  immAttr4fv(outline_col_id, color_outline);
  immVertex2f(pos_id, locx, locy);
}

static void node_socket_draw_multi_input(const float color[4],
                                         const float color_outline[4],
                                         const float width,
                                         const float height,
                                         const float2 location)
{
  /* The other sockets are drawn with the keyframe shader. There, the outline has a base thickness
   * that can be varied but always scales with the size the socket is drawn at. Using `U.dpi_fac`
   * has the same effect here. It scales the outline correctly across different screen DPI's
   * and UI scales without being affected by the 'line-width'. */
  const float outline_width = NODE_SOCK_OUTLINE_SCALE * U.dpi_fac;

  /* UI_draw_roundbox draws the outline on the outer side, so compensate for the outline width. */
  const rctf rect = {
      location.x - width + outline_width * 0.5f,
      location.x + width - outline_width * 0.5f,
      location.y - height + outline_width * 0.5f,
      location.y + height - outline_width * 0.5f,
  };

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv_ex(
      &rect, color, nullptr, 1.0f, color_outline, outline_width, width - outline_width * 0.5f);
}

static const float virtual_node_socket_outline_color[4] = {0.5, 0.5, 0.5, 1.0};

static void node_socket_outline_color_get(const bool selected,
                                          const int socket_type,
                                          float r_outline_color[4])
{
  if (selected) {
    UI_GetThemeColor4fv(TH_ACTIVE, r_outline_color);
  }
  else if (socket_type == SOCK_CUSTOM) {
    /* Until there is a better place for per socket color,
     * the outline color for virtual sockets is set  here. */
    copy_v4_v4(r_outline_color, virtual_node_socket_outline_color);
  }
  else {
    UI_GetThemeColor4fv(TH_WIRE, r_outline_color);
    r_outline_color[3] = 1.0f;
  }
}

void node_socket_color_get(const bContext &C,
                           const bNodeTree &ntree,
                           PointerRNA &node_ptr,
                           const bNodeSocket &sock,
                           float r_color[4])
{
  PointerRNA ptr;
  BLI_assert(RNA_struct_is_a(node_ptr.type, &RNA_Node));
  RNA_pointer_create((ID *)&ntree, &RNA_NodeSocket, &const_cast<bNodeSocket &>(sock), &ptr);

  sock.typeinfo->draw_color((bContext *)&C, &ptr, &node_ptr, r_color);
}

struct SocketTooltipData {
  const bNodeTree *ntree;
  const bNode *node;
  const bNodeSocket *socket;
};

static void create_inspection_string_for_generic_value(const bNodeSocket &socket,
                                                       const GPointer value,
                                                       std::stringstream &ss)
{
  auto id_to_inspection_string = [&](const ID *id, const short idcode) {
    ss << (id ? id->name + 2 : TIP_("None")) << " (" << TIP_(BKE_idtype_idcode_to_name(idcode))
       << ")";
  };

  const CPPType &value_type = *value.type();
  const void *buffer = value.get();
  if (value_type.is<Object *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_OB);
    return;
  }
  else if (value_type.is<Material *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_MA);
    return;
  }
  else if (value_type.is<Tex *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_TE);
    return;
  }
  else if (value_type.is<Image *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_IM);
    return;
  }
  else if (value_type.is<Collection *>()) {
    id_to_inspection_string(*static_cast<const ID *const *>(buffer), ID_GR);
    return;
  }
  else if (value_type.is<std::string>()) {
    ss << *static_cast<const std::string *>(buffer) << TIP_(" (String)");
    return;
  }

  const CPPType &socket_type = *socket.typeinfo->base_cpp_type;
  const bke::DataTypeConversions &convert = bke::get_implicit_type_conversions();
  if (value_type != socket_type) {
    if (!convert.is_convertible(value_type, socket_type)) {
      return;
    }
  }
  BUFFER_FOR_CPP_TYPE_VALUE(socket_type, socket_value);
  /* This will just copy the value if the types are equal. */
  convert.convert_to_uninitialized(value_type, socket_type, buffer, socket_value);
  BLI_SCOPED_DEFER([&]() { socket_type.destruct(socket_value); });

  if (socket_type.is<int>()) {
    ss << *static_cast<int *>(socket_value) << TIP_(" (Integer)");
  }
  else if (socket_type.is<float>()) {
    ss << *static_cast<float *>(socket_value) << TIP_(" (Float)");
  }
  else if (socket_type.is<blender::float3>()) {
    ss << *static_cast<blender::float3 *>(socket_value) << TIP_(" (Vector)");
  }
  else if (socket_type.is<blender::ColorGeometry4f>()) {
    const blender::ColorGeometry4f &color = *static_cast<blender::ColorGeometry4f *>(socket_value);
    ss << "(" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << ")"
       << TIP_(" (Color)");
  }
  else if (socket_type.is<bool>()) {
    ss << ((*static_cast<bool *>(socket_value)) ? TIP_("True") : TIP_("False"))
       << TIP_(" (Boolean)");
  }
}

static void create_inspection_string_for_field_info(const bNodeSocket &socket,
                                                    const geo_log::FieldInfoLog &value_log,
                                                    std::stringstream &ss)
{
  const CPPType &socket_type = *socket.typeinfo->base_cpp_type;
  const Span<std::string> input_tooltips = value_log.input_tooltips;

  if (input_tooltips.is_empty()) {
    /* Should have been logged as constant value. */
    BLI_assert_unreachable();
    ss << "Value has not been logged";
  }
  else {
    if (socket_type.is<int>()) {
      ss << TIP_("Integer field");
    }
    else if (socket_type.is<float>()) {
      ss << TIP_("Float field");
    }
    else if (socket_type.is<blender::float3>()) {
      ss << TIP_("Vector field");
    }
    else if (socket_type.is<bool>()) {
      ss << TIP_("Boolean field");
    }
    else if (socket_type.is<std::string>()) {
      ss << TIP_("String field");
    }
    else if (socket_type.is<blender::ColorGeometry4f>()) {
      ss << TIP_("Color field");
    }
    ss << TIP_(" based on:\n");

    for (const int i : input_tooltips.index_range()) {
      const blender::StringRef tooltip = input_tooltips[i];
      ss << "\u2022 " << tooltip;
      if (i < input_tooltips.size() - 1) {
        ss << ".\n";
      }
    }
  }
}

static void create_inspection_string_for_geometry_info(const geo_log::GeometryInfoLog &value_log,
                                                       std::stringstream &ss,
                                                       const nodes::decl::Geometry *socket_decl)
{
  Span<GeometryComponentType> component_types = value_log.component_types;
  if (component_types.is_empty()) {
    ss << TIP_("Empty Geometry");
    return;
  }

  auto to_string = [](int value) {
    char str[16];
    BLI_str_format_int_grouped(str, value);
    return std::string(str);
  };

  ss << TIP_("Geometry:\n");
  for (GeometryComponentType type : component_types) {
    const char *line_end = (type == component_types.last()) ? "" : ".\n";
    switch (type) {
      case GEO_COMPONENT_TYPE_MESH: {
        const geo_log::GeometryInfoLog::MeshInfo &mesh_info = *value_log.mesh_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Mesh: %s vertices, %s edges, %s faces"),
                     to_string(mesh_info.verts_num).c_str(),
                     to_string(mesh_info.edges_num).c_str(),
                     to_string(mesh_info.faces_num).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        const geo_log::GeometryInfoLog::PointCloudInfo &pointcloud_info =
            *value_log.pointcloud_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Point Cloud: %s points"),
                     to_string(pointcloud_info.points_num).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_CURVE: {
        const geo_log::GeometryInfoLog::CurveInfo &curve_info = *value_log.curve_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Curve: %s splines"),
                     to_string(curve_info.splines_num).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_INSTANCES: {
        const geo_log::GeometryInfoLog::InstancesInfo &instances_info = *value_log.instances_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Instances: %s"),
                     to_string(instances_info.instances_num).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_VOLUME: {
        ss << TIP_("\u2022 Volume") << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_EDIT: {
        if (value_log.edit_data_info.has_value()) {
          const geo_log::GeometryInfoLog::EditDataInfo &edit_info = *value_log.edit_data_info;
          char line[256];
          BLI_snprintf(line,
                       sizeof(line),
                       TIP_("\u2022 Edit Curves: %s, %s"),
                       edit_info.has_deformed_positions ? TIP_("positions") : TIP_("no positions"),
                       edit_info.has_deform_matrices ? TIP_("matrices") : TIP_("no matrices"));
          ss << line << line_end;
        }
        break;
      }
    }
  }

  /* If the geometry declaration is null, as is the case for input to group output,
   * or it is an output socket don't show supported types. */
  if (socket_decl == nullptr || socket_decl->in_out() == SOCK_OUT) {
    return;
  }

  Span<GeometryComponentType> supported_types = socket_decl->supported_types();
  if (supported_types.is_empty()) {
    ss << ".\n\n" << TIP_("Supported: All Types");
    return;
  }

  ss << ".\n\n" << TIP_("Supported: ");
  for (GeometryComponentType type : supported_types) {
    switch (type) {
      case GEO_COMPONENT_TYPE_MESH: {
        ss << TIP_("Mesh");
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        ss << TIP_("Point Cloud");
        break;
      }
      case GEO_COMPONENT_TYPE_CURVE: {
        ss << TIP_("Curve");
        break;
      }
      case GEO_COMPONENT_TYPE_INSTANCES: {
        ss << TIP_("Instances");
        break;
      }
      case GEO_COMPONENT_TYPE_VOLUME: {
        ss << TIP_("Volume");
        break;
      }
      case GEO_COMPONENT_TYPE_EDIT: {
        break;
      }
    }
    ss << ((type == supported_types.last()) ? "" : ", ");
  }
}

static std::optional<std::string> create_socket_inspection_string(TreeDrawContext &tree_draw_ctx,
                                                                  const bNodeSocket &socket)
{
  using namespace blender::nodes::geo_eval_log;

  if (socket.typeinfo->base_cpp_type == nullptr) {
    return std::nullopt;
  }

  tree_draw_ctx.geo_tree_log->ensure_socket_values();
  ValueLog *value_log = tree_draw_ctx.geo_tree_log->find_socket_value_log(socket);
  if (value_log == nullptr) {
    return std::nullopt;
  }
  std::stringstream ss;
  if (const geo_log::GenericValueLog *generic_value_log =
          dynamic_cast<const geo_log::GenericValueLog *>(value_log)) {
    create_inspection_string_for_generic_value(socket, generic_value_log->value, ss);
  }
  else if (const geo_log::FieldInfoLog *gfield_value_log =
               dynamic_cast<const geo_log::FieldInfoLog *>(value_log)) {
    create_inspection_string_for_field_info(socket, *gfield_value_log, ss);
  }
  else if (const geo_log::GeometryInfoLog *geo_value_log =
               dynamic_cast<const geo_log::GeometryInfoLog *>(value_log)) {
    create_inspection_string_for_geometry_info(
        *geo_value_log,
        ss,
        dynamic_cast<const nodes::decl::Geometry *>(socket.runtime->declaration));
  }

  std::string str = ss.str();
  if (str.empty()) {
    return std::nullopt;
  }
  return str;
}

static bool node_socket_has_tooltip(const bNodeTree &ntree, const bNodeSocket &socket)
{
  if (ntree.type == NTREE_GEOMETRY) {
    return true;
  }

  if (socket.runtime->declaration != nullptr) {
    const nodes::SocketDeclaration &socket_decl = *socket.runtime->declaration;
    return !socket_decl.description().is_empty();
  }

  return false;
}

static char *node_socket_get_tooltip(const bContext *C,
                                     const bNodeTree *ntree,
                                     const bNode * /*node*/,
                                     const bNodeSocket *socket)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  TreeDrawContext tree_draw_ctx;
  if (snode != nullptr) {
    if (ntree->type == NTREE_GEOMETRY) {
      tree_draw_ctx.geo_tree_log = geo_log::GeoModifierLog::get_tree_log_for_node_editor(*snode);
    }
  }

  std::stringstream output;
  if (socket->runtime->declaration != nullptr) {
    const blender::nodes::SocketDeclaration &socket_decl = *socket->runtime->declaration;
    blender::StringRef description = socket_decl.description();
    if (!description.is_empty()) {
      output << TIP_(description.data());
    }
  }

  if (ntree->type == NTREE_GEOMETRY && tree_draw_ctx.geo_tree_log != nullptr) {
    if (!output.str().empty()) {
      output << ".\n\n";
    }

    std::optional<std::string> socket_inspection_str = create_socket_inspection_string(
        tree_draw_ctx, *socket);
    if (socket_inspection_str.has_value()) {
      output << *socket_inspection_str;
    }
    else {
      output << TIP_("The socket value has not been computed yet");
    }
  }

  if (output.str().empty()) {
    output << nodeSocketLabel(socket);
  }

  return BLI_strdup(output.str().c_str());
}

static void node_socket_add_tooltip_in_node_editor(TreeDrawContext * /*tree_draw_ctx*/,
                                                   const bNodeTree *ntree,
                                                   const bNode *node,
                                                   const bNodeSocket *sock,
                                                   uiLayout *layout)
{
  if (!node_socket_has_tooltip(*ntree, *sock)) {
    return;
  }

  SocketTooltipData *data = MEM_cnew<SocketTooltipData>(__func__);
  data->ntree = ntree;
  data->node = node;
  data->socket = sock;

  uiLayoutSetTooltipFunc(
      layout,
      [](bContext *C, void *argN, const char * /*tip*/) {
        SocketTooltipData *data = static_cast<SocketTooltipData *>(argN);
        return node_socket_get_tooltip(C, data->ntree, data->node, data->socket);
      },
      data,
      MEM_dupallocN,
      MEM_freeN);
}

void node_socket_add_tooltip(const bNodeTree &ntree,
                             const bNode &node,
                             const bNodeSocket &sock,
                             uiLayout &layout)
{
  node_socket_add_tooltip_in_node_editor(nullptr, &ntree, &node, &sock, &layout);
}

static void node_socket_draw_nested(const bContext &C,
                                    bNodeTree &ntree,
                                    PointerRNA &node_ptr,
                                    uiBlock &block,
                                    bNodeSocket &sock,
                                    const uint pos_id,
                                    const uint col_id,
                                    const uint shape_id,
                                    const uint size_id,
                                    const uint outline_col_id,
                                    const float size,
                                    const bool selected)
{
  const float2 location(sock.runtime->locx, sock.runtime->locy);

  float color[4];
  float outline_color[4];
  node_socket_color_get(C, ntree, node_ptr, sock, color);
  node_socket_outline_color_get(selected, sock.type, outline_color);

  node_socket_draw(sock,
                   color,
                   outline_color,
                   size,
                   location.x,
                   location.y,
                   pos_id,
                   col_id,
                   shape_id,
                   size_id,
                   outline_col_id);

  if (!node_socket_has_tooltip(ntree, sock)) {
    return;
  }

  /* Ideally sockets themselves should be buttons, but they aren't currently. So add an invisible
   * button on top of them for the tooltip. */
  const eUIEmbossType old_emboss = UI_block_emboss_get(&block);
  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiBut *but = uiDefIconBut(&block,
                            UI_BTYPE_BUT,
                            0,
                            ICON_NONE,
                            location.x - size / 2.0f,
                            location.y - size / 2.0f,
                            size,
                            size,
                            nullptr,
                            0,
                            0,
                            0,
                            0,
                            nullptr);

  SocketTooltipData *data = MEM_new<SocketTooltipData>(__func__);
  data->ntree = &ntree;
  data->node = static_cast<const bNode *>(node_ptr.data);
  data->socket = &sock;

  UI_but_func_tooltip_set(
      but,
      [](bContext *C, void *argN, const char * /*tip*/) {
        SocketTooltipData *data = (SocketTooltipData *)argN;
        return node_socket_get_tooltip(C, data->ntree, data->node, data->socket);
      },
      data,
      MEM_freeN);
  /* Disable the button so that clicks on it are ignored the link operator still works. */
  UI_but_flag_enable(but, UI_BUT_DISABLED);
  UI_block_emboss_set(&block, old_emboss);
}

}  // namespace blender::ed::space_node

void ED_node_socket_draw(bNodeSocket *sock, const rcti *rect, const float color[4], float scale)
{
  using namespace blender::ed::space_node;

  const float size = NODE_SOCKSIZE_DRAW_MULIPLIER * NODE_SOCKSIZE * scale;
  rcti draw_rect = *rect;
  float outline_color[4] = {0};

  node_socket_outline_color_get(sock->flag & SELECT, sock->type, outline_color);

  BLI_rcti_resize(&draw_rect, size, size);

  GPUVertFormat *format = immVertexFormat();
  uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  uint shape_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);
  uint size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  uint outline_col_id = GPU_vertformat_attr_add(
      format, "outlineColor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  eGPUBlend state = GPU_blend_get();
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_program_point_size(true);

  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", NODE_SOCK_OUTLINE_SCALE);
  immUniform2f("ViewportSize", -1.0f, -1.0f);

  /* Single point. */
  immBegin(GPU_PRIM_POINTS, 1);
  node_socket_draw(*sock,
                   color,
                   outline_color,
                   BLI_rcti_size_y(&draw_rect),
                   BLI_rcti_cent_x(&draw_rect),
                   BLI_rcti_cent_y(&draw_rect),
                   pos_id,
                   col_id,
                   shape_id,
                   size_id,
                   outline_col_id);
  immEnd();

  immUnbindProgram();
  GPU_program_point_size(false);

  /* Restore. */
  GPU_blend(state);
}

namespace blender::ed::space_node {

/* **************  Socket callbacks *********** */

static void node_draw_preview_background(rctf *rect)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

  /* Drawing the checkerboard. */
  const float checker_dark = UI_ALPHA_CHECKER_DARK / 255.0f;
  const float checker_light = UI_ALPHA_CHECKER_LIGHT / 255.0f;
  immUniform4f("color1", checker_dark, checker_dark, checker_dark, 1.0f);
  immUniform4f("color2", checker_light, checker_light, checker_light, 1.0f);
  immUniform1i("size", 8);
  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
  immUnbindProgram();
}

/* Not a callback. */
static void node_draw_preview(bNodePreview *preview, rctf *prv)
{
  float xrect = BLI_rctf_size_x(prv);
  float yrect = BLI_rctf_size_y(prv);
  float xscale = xrect / float(preview->xsize);
  float yscale = yrect / float(preview->ysize);
  float scale;

  /* Uniform scale and offset. */
  rctf draw_rect = *prv;
  if (xscale < yscale) {
    float offset = 0.5f * (yrect - float(preview->ysize) * xscale);
    draw_rect.ymin += offset;
    draw_rect.ymax -= offset;
    scale = xscale;
  }
  else {
    float offset = 0.5f * (xrect - float(preview->xsize) * yscale);
    draw_rect.xmin += offset;
    draw_rect.xmax -= offset;
    scale = yscale;
  }

  node_draw_preview_background(&draw_rect);

  GPU_blend(GPU_BLEND_ALPHA);
  /* Premul graphics. */
  GPU_blend(GPU_BLEND_ALPHA);

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
  immDrawPixelsTexTiled(&state,
                        draw_rect.xmin,
                        draw_rect.ymin,
                        preview->xsize,
                        preview->ysize,
                        GPU_RGBA8,
                        true,
                        preview->rect,
                        scale,
                        scale,
                        nullptr);

  GPU_blend(GPU_BLEND_NONE);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColorShadeAlpha(TH_BACK, -15, +100);
  imm_draw_box_wire_2d(pos, draw_rect.xmin, draw_rect.ymin, draw_rect.xmax, draw_rect.ymax);
  immUnbindProgram();
}

/* Common handle function for operator buttons that need to select the node first. */
static void node_toggle_button_cb(bContext *C, void *node_argv, void *op_argv)
{
  bNode *node = (bNode *)node_argv;
  const char *opname = (const char *)op_argv;

  /* Select & activate only the button's node. */
  node_select_single(*C, *node);

  WM_operator_name_call(C, opname, WM_OP_INVOKE_DEFAULT, nullptr, nullptr);
}

static void node_draw_shadow(const SpaceNode &snode,
                             const bNode &node,
                             const float radius,
                             const float alpha)
{
  const rctf &rct = node.runtime->totr;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  ui_draw_dropshadow(&rct, radius, snode.runtime->aspect, alpha, node.flag & SELECT);
}

static void node_draw_sockets(const View2D &v2d,
                              const bContext &C,
                              bNodeTree &ntree,
                              bNode &node,
                              uiBlock &block,
                              const bool draw_outputs,
                              const bool select_all)
{
  const uint total_input_len = BLI_listbase_count(&node.inputs);
  const uint total_output_len = BLI_listbase_count(&node.outputs);

  if (total_input_len + total_output_len == 0) {
    return;
  }

  PointerRNA node_ptr;
  RNA_pointer_create((ID *)&ntree, &RNA_Node, &node, &node_ptr);

  bool selected = false;

  GPUVertFormat *format = immVertexFormat();
  uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  uint shape_id = GPU_vertformat_attr_add(format, "flags", GPU_COMP_U32, 1, GPU_FETCH_INT);
  uint size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  uint outline_col_id = GPU_vertformat_attr_add(
      format, "outlineColor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_KEYFRAME_SHAPE);
  immUniform1f("outline_scale", NODE_SOCK_OUTLINE_SCALE);
  immUniform2f("ViewportSize", -1.0f, -1.0f);

  /* Set handle size. */
  const float socket_draw_size = NODE_SOCKSIZE * NODE_SOCKSIZE_DRAW_MULIPLIER;
  float scale;
  UI_view2d_scale_get(&v2d, &scale, nullptr);
  scale *= socket_draw_size;

  if (!select_all) {
    immBeginAtMost(GPU_PRIM_POINTS, total_input_len + total_output_len);
  }

  /* Socket inputs. */
  short selected_input_len = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, &node.inputs) {
    if (nodeSocketIsHidden(sock)) {
      continue;
    }
    if (select_all || (sock->flag & SELECT)) {
      if (!(sock->flag & SOCK_MULTI_INPUT)) {
        /* Don't add multi-input sockets here since they are drawn in a different batch. */
        selected_input_len++;
      }
      continue;
    }
    /* Don't draw multi-input sockets here since they are drawn in a different batch. */
    if (sock->flag & SOCK_MULTI_INPUT) {
      continue;
    }

    node_socket_draw_nested(C,
                            ntree,
                            node_ptr,
                            block,
                            *sock,
                            pos_id,
                            col_id,
                            shape_id,
                            size_id,
                            outline_col_id,
                            scale,
                            selected);
  }

  /* Socket outputs. */
  short selected_output_len = 0;
  if (draw_outputs) {
    LISTBASE_FOREACH (bNodeSocket *, sock, &node.outputs) {
      if (nodeSocketIsHidden(sock)) {
        continue;
      }
      if (select_all || (sock->flag & SELECT)) {
        selected_output_len++;
        continue;
      }

      node_socket_draw_nested(C,
                              ntree,
                              node_ptr,
                              block,
                              *sock,
                              pos_id,
                              col_id,
                              shape_id,
                              size_id,
                              outline_col_id,
                              scale,
                              selected);
    }
  }

  if (!select_all) {
    immEnd();
  }

  /* Go back and draw selected sockets. */
  if (selected_input_len + selected_output_len > 0) {
    /* Outline for selected sockets. */

    selected = true;

    immBegin(GPU_PRIM_POINTS, selected_input_len + selected_output_len);

    if (selected_input_len) {
      /* Socket inputs. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node.inputs) {
        if (nodeSocketIsHidden(sock)) {
          continue;
        }
        /* Don't draw multi-input sockets here since they are drawn in a different batch. */
        if (sock->flag & SOCK_MULTI_INPUT) {
          continue;
        }
        if (select_all || (sock->flag & SELECT)) {
          node_socket_draw_nested(C,
                                  ntree,
                                  node_ptr,
                                  block,
                                  *sock,
                                  pos_id,
                                  col_id,
                                  shape_id,
                                  size_id,
                                  outline_col_id,
                                  scale,
                                  selected);
          if (--selected_input_len == 0) {
            break; /* Stop as soon as last one is drawn. */
          }
        }
      }
    }

    if (selected_output_len) {
      /* Socket outputs. */
      LISTBASE_FOREACH (bNodeSocket *, sock, &node.outputs) {
        if (nodeSocketIsHidden(sock)) {
          continue;
        }
        if (select_all || (sock->flag & SELECT)) {
          node_socket_draw_nested(C,
                                  ntree,
                                  node_ptr,
                                  block,
                                  *sock,
                                  pos_id,
                                  col_id,
                                  shape_id,
                                  size_id,
                                  outline_col_id,
                                  scale,
                                  selected);
          if (--selected_output_len == 0) {
            break; /* Stop as soon as last one is drawn. */
          }
        }
      }
    }

    immEnd();
  }

  immUnbindProgram();

  GPU_program_point_size(false);
  GPU_blend(GPU_BLEND_NONE);

  /* Draw multi-input sockets after the others because they are drawn with `UI_draw_roundbox`
   * rather than with `GL_POINT`. */
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (nodeSocketIsHidden(socket)) {
      continue;
    }
    if (!(socket->flag & SOCK_MULTI_INPUT)) {
      continue;
    }

    const bool is_node_hidden = (node.flag & NODE_HIDDEN);
    const float width = 0.5f * socket_draw_size;
    float height = is_node_hidden ? width : node_socket_calculate_height(*socket) - width;

    float color[4];
    float outline_color[4];
    node_socket_color_get(C, ntree, node_ptr, *socket, color);
    node_socket_outline_color_get(socket->flag & SELECT, socket->type, outline_color);

    const float2 location(socket->runtime->locx, socket->runtime->locy);
    node_socket_draw_multi_input(color, outline_color, width, height, location);
  }
}

static int node_error_type_to_icon(const geo_log::NodeWarningType type)
{
  switch (type) {
    case geo_log::NodeWarningType::Error:
      return ICON_ERROR;
    case geo_log::NodeWarningType::Warning:
      return ICON_ERROR;
    case geo_log::NodeWarningType::Info:
      return ICON_INFO;
  }

  BLI_assert(false);
  return ICON_ERROR;
}

static uint8_t node_error_type_priority(const geo_log::NodeWarningType type)
{
  switch (type) {
    case geo_log::NodeWarningType::Error:
      return 3;
    case geo_log::NodeWarningType::Warning:
      return 2;
    case geo_log::NodeWarningType::Info:
      return 1;
  }

  BLI_assert(false);
  return 0;
}

static geo_log::NodeWarningType node_error_highest_priority(Span<geo_log::NodeWarning> warnings)
{
  uint8_t highest_priority = 0;
  geo_log::NodeWarningType highest_priority_type = geo_log::NodeWarningType::Info;
  for (const geo_log::NodeWarning &warning : warnings) {
    const uint8_t priority = node_error_type_priority(warning.type);
    if (priority > highest_priority) {
      highest_priority = priority;
      highest_priority_type = warning.type;
    }
  }
  return highest_priority_type;
}

struct NodeErrorsTooltipData {
  Span<geo_log::NodeWarning> warnings;
};

static char *node_errors_tooltip_fn(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  NodeErrorsTooltipData &data = *(NodeErrorsTooltipData *)argN;

  std::string complete_string;

  for (const geo_log::NodeWarning &warning : data.warnings.drop_back(1)) {
    complete_string += warning.message;
    /* Adding the period is not ideal for multi-line messages, but it is consistent
     * with other tooltip implementations in Blender, so it is added here. */
    complete_string += '.';
    complete_string += '\n';
  }

  /* Let the tooltip system automatically add the last period. */
  complete_string += data.warnings.last().message;

  return BLI_strdupn(complete_string.c_str(), complete_string.size());
}

#define NODE_HEADER_ICON_SIZE (0.8f * U.widget_unit)

static void node_add_unsupported_compositor_operation_error_message_button(bNode &node,
                                                                           uiBlock &block,
                                                                           const rctf &rect,
                                                                           float &icon_offset)
{
  icon_offset -= NODE_HEADER_ICON_SIZE;
  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiDefIconBut(&block,
               UI_BTYPE_BUT,
               0,
               ICON_ERROR,
               icon_offset,
               rect.ymax - NODE_DY,
               NODE_HEADER_ICON_SIZE,
               UI_UNIT_Y,
               nullptr,
               0,
               0,
               0,
               0,
               TIP_(node.typeinfo->realtime_compositor_unsupported_message));
  UI_block_emboss_set(&block, UI_EMBOSS);
}

static void node_add_error_message_button(TreeDrawContext &tree_draw_ctx,
                                          bNode &node,
                                          uiBlock &block,
                                          const rctf &rect,
                                          float &icon_offset)
{
  if (tree_draw_ctx.used_by_realtime_compositor &&
      node.typeinfo->realtime_compositor_unsupported_message) {
    node_add_unsupported_compositor_operation_error_message_button(node, block, rect, icon_offset);
    return;
  }

  Span<geo_log::NodeWarning> warnings;
  if (tree_draw_ctx.geo_tree_log) {
    geo_log::GeoNodeLog *node_log = tree_draw_ctx.geo_tree_log->nodes.lookup_ptr(node.identifier);
    if (node_log != nullptr) {
      warnings = node_log->warnings;
    }
  }
  if (warnings.is_empty()) {
    return;
  }

  const geo_log::NodeWarningType display_type = node_error_highest_priority(warnings);
  NodeErrorsTooltipData *tooltip_data = MEM_new<NodeErrorsTooltipData>(__func__);
  tooltip_data->warnings = warnings;

  icon_offset -= NODE_HEADER_ICON_SIZE;
  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiBut *but = uiDefIconBut(&block,
                            UI_BTYPE_BUT,
                            0,
                            node_error_type_to_icon(display_type),
                            icon_offset,
                            rect.ymax - NODE_DY,
                            NODE_HEADER_ICON_SIZE,
                            UI_UNIT_Y,
                            nullptr,
                            0,
                            0,
                            0,
                            0,
                            nullptr);
  UI_but_func_tooltip_set(but, node_errors_tooltip_fn, tooltip_data, [](void *arg) {
    MEM_delete(static_cast<NodeErrorsTooltipData *>(arg));
  });
  UI_block_emboss_set(&block, UI_EMBOSS);
}

static std::optional<std::chrono::nanoseconds> node_get_execution_time(
    TreeDrawContext &tree_draw_ctx, const bNodeTree &ntree, const bNode &node)
{
  const geo_log::GeoTreeLog *tree_log = tree_draw_ctx.geo_tree_log;
  if (tree_log == nullptr) {
    return std::nullopt;
  }
  if (node.type == NODE_GROUP_OUTPUT) {
    return tree_log->run_time_sum;
  }
  else if (node.is_frame()) {
    /* Could be cached in the future if this recursive code turns out to be slow. */
    std::chrono::nanoseconds run_time{0};
    bool found_node = false;

    for (const bNode *tnode : node.direct_children_in_frame()) {
      if (tnode->is_frame()) {
        std::optional<std::chrono::nanoseconds> sub_frame_run_time = node_get_execution_time(
            tree_draw_ctx, ntree, *tnode);
        if (sub_frame_run_time.has_value()) {
          run_time += *sub_frame_run_time;
          found_node = true;
        }
      }
      else {
        if (const geo_log::GeoNodeLog *node_log = tree_log->nodes.lookup_ptr_as(
                tnode->identifier)) {
          found_node = true;
          run_time += node_log->run_time;
        }
      }
    }
    if (found_node) {
      return run_time;
    }
    return std::nullopt;
  }
  if (const geo_log::GeoNodeLog *node_log = tree_log->nodes.lookup_ptr(node.identifier)) {
    return node_log->run_time;
  }
  return std::nullopt;
}

static std::string node_get_execution_time_label(TreeDrawContext &tree_draw_ctx,
                                                 const SpaceNode &snode,
                                                 const bNode &node)
{
  const std::optional<std::chrono::nanoseconds> exec_time = node_get_execution_time(
      tree_draw_ctx, *snode.edittree, node);

  if (!exec_time.has_value()) {
    return std::string("");
  }

  const uint64_t exec_time_us =
      std::chrono::duration_cast<std::chrono::microseconds>(*exec_time).count();

  /* Don't show time if execution time is 0 microseconds. */
  if (exec_time_us == 0) {
    return std::string("-");
  }
  if (exec_time_us < 100) {
    return std::string("< 0.1 ms");
  }

  int precision = 0;
  /* Show decimal if value is below 1ms */
  if (exec_time_us < 1000) {
    precision = 2;
  }
  else if (exec_time_us < 10000) {
    precision = 1;
  }

  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << (exec_time_us / 1000.0f);
  return stream.str() + " ms";
}

struct NodeExtraInfoRow {
  std::string text;
  int icon;
  const char *tooltip = nullptr;

  uiButToolTipFunc tooltip_fn = nullptr;
  void *tooltip_fn_arg = nullptr;
  void (*tooltip_fn_free_arg)(void *) = nullptr;
};

struct NamedAttributeTooltipArg {
  Map<StringRefNull, geo_log::NamedAttributeUsage> usage_by_attribute;
};

static char *named_attribute_tooltip(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  NamedAttributeTooltipArg &arg = *static_cast<NamedAttributeTooltipArg *>(argN);

  std::stringstream ss;
  ss << TIP_("Accessed named attributes:\n");

  struct NameWithUsage {
    StringRefNull name;
    geo_log::NamedAttributeUsage usage;
  };

  Vector<NameWithUsage> sorted_used_attribute;
  for (auto &&item : arg.usage_by_attribute.items()) {
    sorted_used_attribute.append({item.key, item.value});
  }
  std::sort(sorted_used_attribute.begin(),
            sorted_used_attribute.end(),
            [](const NameWithUsage &a, const NameWithUsage &b) {
              return BLI_strcasecmp_natural(a.name.c_str(), b.name.c_str()) <= 0;
            });

  for (const NameWithUsage &attribute : sorted_used_attribute) {
    const StringRefNull name = attribute.name;
    const geo_log::NamedAttributeUsage usage = attribute.usage;
    ss << "  \u2022 \"" << name << "\": ";
    Vector<std::string> usages;
    if ((usage & geo_log::NamedAttributeUsage::Read) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("read"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Write) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("write"));
    }
    if ((usage & geo_log::NamedAttributeUsage::Remove) != geo_log::NamedAttributeUsage::None) {
      usages.append(TIP_("remove"));
    }
    for (const int i : usages.index_range()) {
      ss << usages[i];
      if (i < usages.size() - 1) {
        ss << ", ";
      }
    }
    ss << "\n";
  }
  ss << "\n";
  ss << TIP_(
      "Attributes with these names used within the group may conflict with existing attributes");
  return BLI_strdup(ss.str().c_str());
}

static NodeExtraInfoRow row_from_used_named_attribute(
    const Map<StringRefNull, geo_log::NamedAttributeUsage> &usage_by_attribute_name)
{
  const int attributes_num = usage_by_attribute_name.size();

  NodeExtraInfoRow row;
  row.text = std::to_string(attributes_num) +
             TIP_(attributes_num == 1 ? " Named Attribute" : " Named Attributes");
  row.icon = ICON_SPREADSHEET;
  row.tooltip_fn = named_attribute_tooltip;
  row.tooltip_fn_arg = new NamedAttributeTooltipArg{usage_by_attribute_name};
  row.tooltip_fn_free_arg = [](void *arg) { delete static_cast<NamedAttributeTooltipArg *>(arg); };
  return row;
}

static std::optional<NodeExtraInfoRow> node_get_accessed_attributes_row(
    TreeDrawContext &tree_draw_ctx, const bNode &node)
{
  if (tree_draw_ctx.geo_tree_log == nullptr) {
    return std::nullopt;
  }
  if (ELEM(node.type,
           GEO_NODE_STORE_NAMED_ATTRIBUTE,
           GEO_NODE_REMOVE_ATTRIBUTE,
           GEO_NODE_INPUT_NAMED_ATTRIBUTE)) {
    /* Only show the overlay when the name is passed in from somewhere else. */
    LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
      if (STREQ(socket->name, "Name")) {
        if (!socket->is_directly_linked()) {
          return std::nullopt;
        }
      }
    }
  }
  tree_draw_ctx.geo_tree_log->ensure_used_named_attributes();
  geo_log::GeoNodeLog *node_log = tree_draw_ctx.geo_tree_log->nodes.lookup_ptr(node.identifier);
  if (node_log == nullptr) {
    return std::nullopt;
  }
  if (node_log->used_named_attributes.is_empty()) {
    return std::nullopt;
  }
  return row_from_used_named_attribute(node_log->used_named_attributes);
}

static Vector<NodeExtraInfoRow> node_get_extra_info(TreeDrawContext &tree_draw_ctx,
                                                    const SpaceNode &snode,
                                                    const bNode &node)
{
  Vector<NodeExtraInfoRow> rows;
  if (!(snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS)) {
    return rows;
  }

  if (snode.overlay.flag & SN_OVERLAY_SHOW_NAMED_ATTRIBUTES &&
      snode.edittree->type == NTREE_GEOMETRY) {
    if (std::optional<NodeExtraInfoRow> row = node_get_accessed_attributes_row(tree_draw_ctx,
                                                                               node)) {
      rows.append(std::move(*row));
    }
  }

  if (snode.overlay.flag & SN_OVERLAY_SHOW_TIMINGS && snode.edittree->type == NTREE_GEOMETRY &&
      (ELEM(node.typeinfo->nclass, NODE_CLASS_GEOMETRY, NODE_CLASS_GROUP, NODE_CLASS_ATTRIBUTE) ||
       ELEM(node.type, NODE_FRAME, NODE_GROUP_OUTPUT))) {
    NodeExtraInfoRow row;
    row.text = node_get_execution_time_label(tree_draw_ctx, snode, node);
    if (!row.text.empty()) {
      row.tooltip = TIP_(
          "The execution time from the node tree's latest evaluation. For frame and group nodes, "
          "the time for all sub-nodes");
      row.icon = ICON_PREVIEW_RANGE;
      rows.append(std::move(row));
    }
  }

  if (snode.edittree->type == NTREE_GEOMETRY) {
    if (geo_log::GeoTreeLog *tree_log = tree_draw_ctx.geo_tree_log) {
      tree_log->ensure_debug_messages();
      const geo_log::GeoNodeLog *node_log = tree_log->nodes.lookup_ptr(node.identifier);
      if (node_log != nullptr) {
        for (const StringRef message : node_log->debug_messages) {
          NodeExtraInfoRow row;
          row.text = message;
          row.icon = ICON_INFO;
          rows.append(std::move(row));
        }
      }
    }
  }

  return rows;
}

static void node_draw_extra_info_row(const bNode &node,
                                     uiBlock &block,
                                     const rctf &rect,
                                     const int row,
                                     const NodeExtraInfoRow &extra_info_row)
{
  const float but_icon_left = rect.xmin + 6.0f * U.dpi_fac;
  const float but_icon_width = NODE_HEADER_ICON_SIZE * 0.8f;
  const float but_icon_right = but_icon_left + but_icon_width;

  UI_block_emboss_set(&block, UI_EMBOSS_NONE);
  uiBut *but_icon = uiDefIconBut(&block,
                                 UI_BTYPE_BUT,
                                 0,
                                 extra_info_row.icon,
                                 int(but_icon_left),
                                 int(rect.ymin + row * (20.0f * U.dpi_fac)),
                                 but_icon_width,
                                 UI_UNIT_Y,
                                 nullptr,
                                 0,
                                 0,
                                 0,
                                 0,
                                 extra_info_row.tooltip);
  if (extra_info_row.tooltip_fn != nullptr) {
    UI_but_func_tooltip_set(but_icon,
                            extra_info_row.tooltip_fn,
                            extra_info_row.tooltip_fn_arg,
                            extra_info_row.tooltip_fn_free_arg);
  }
  UI_block_emboss_set(&block, UI_EMBOSS);

  const float but_text_left = but_icon_right + 6.0f * U.dpi_fac;
  const float but_text_right = rect.xmax;
  const float but_text_width = but_text_right - but_text_left;

  uiBut *but_text = uiDefBut(&block,
                             UI_BTYPE_LABEL,
                             0,
                             extra_info_row.text.c_str(),
                             int(but_text_left),
                             int(rect.ymin + row * (20.0f * U.dpi_fac)),
                             short(but_text_width),
                             short(NODE_DY),
                             nullptr,
                             0,
                             0,
                             0,
                             0,
                             "");

  if (node.flag & NODE_MUTED) {
    UI_but_flag_enable(but_text, UI_BUT_INACTIVE);
    UI_but_flag_enable(but_icon, UI_BUT_INACTIVE);
  }
}

static void node_draw_extra_info_panel(TreeDrawContext &tree_draw_ctx,
                                       const SpaceNode &snode,
                                       const bNode &node,
                                       uiBlock &block)
{
  Vector<NodeExtraInfoRow> extra_info_rows = node_get_extra_info(tree_draw_ctx, snode, node);

  if (extra_info_rows.size() == 0) {
    return;
  }

  const rctf &rct = node.runtime->totr;
  float color[4];
  rctf extra_info_rect;

  const float width = (node.width - 6.0f) * U.dpi_fac;

  if (node.type == NODE_FRAME) {
    extra_info_rect.xmin = rct.xmin;
    extra_info_rect.xmax = rct.xmin + 95.0f * U.dpi_fac;
    extra_info_rect.ymin = rct.ymin + 2.0f * U.dpi_fac;
    extra_info_rect.ymax = rct.ymin + 2.0f * U.dpi_fac;
  }
  else {
    extra_info_rect.xmin = rct.xmin + 3.0f * U.dpi_fac;
    extra_info_rect.xmax = rct.xmin + width;
    extra_info_rect.ymin = rct.ymax;
    extra_info_rect.ymax = rct.ymax + extra_info_rows.size() * (20.0f * U.dpi_fac);

    if (node.flag & NODE_MUTED) {
      UI_GetThemeColorBlend4f(TH_BACK, TH_NODE, 0.2f, color);
    }
    else {
      UI_GetThemeColorBlend4f(TH_BACK, TH_NODE, 0.75f, color);
    }
    color[3] -= 0.35f;
    UI_draw_roundbox_corner_set(
        UI_CNR_ALL & ~UI_CNR_BOTTOM_LEFT &
        ((rct.xmax) > extra_info_rect.xmax ? ~UI_CNR_BOTTOM_RIGHT : UI_CNR_ALL));
    UI_draw_roundbox_4fv(&extra_info_rect, true, BASIS_RAD, color);

    /* Draw outline. */
    const float outline_width = 1.0f;
    extra_info_rect.xmin = rct.xmin + 3.0f * U.dpi_fac - outline_width;
    extra_info_rect.xmax = rct.xmin + width + outline_width;
    extra_info_rect.ymin = rct.ymax - outline_width;
    extra_info_rect.ymax = rct.ymax + outline_width + extra_info_rows.size() * (20.0f * U.dpi_fac);

    UI_GetThemeColorBlendShade4fv(TH_BACK, TH_NODE, 0.4f, -20, color);
    UI_draw_roundbox_corner_set(
        UI_CNR_ALL & ~UI_CNR_BOTTOM_LEFT &
        ((rct.xmax) > extra_info_rect.xmax ? ~UI_CNR_BOTTOM_RIGHT : UI_CNR_ALL));
    UI_draw_roundbox_4fv(&extra_info_rect, false, BASIS_RAD, color);
  }

  for (int row : extra_info_rows.index_range()) {
    node_draw_extra_info_row(node, block, extra_info_rect, row, extra_info_rows[row]);
  }
}

static void node_draw_basis(const bContext &C,
                            TreeDrawContext &tree_draw_ctx,
                            const View2D &v2d,
                            const SpaceNode &snode,
                            bNodeTree &ntree,
                            bNode &node,
                            uiBlock &block,
                            bNodeInstanceKey key)
{
  const float iconbutw = NODE_HEADER_ICON_SIZE;

  /* Skip if out of view. */
  if (BLI_rctf_isect(&node.runtime->totr, &v2d.cur, nullptr) == false) {
    UI_block_end(&C, &block);
    return;
  }

  /* Shadow. */
  node_draw_shadow(snode, node, BASIS_RAD, 1.0f);

  const rctf &rct = node.runtime->totr;
  float color[4];
  int color_id = node_get_colorid(tree_draw_ctx, node);

  GPU_line_width(1.0f);

  node_draw_extra_info_panel(tree_draw_ctx, snode, node, block);

  /* Header. */
  {
    const rctf rect = {
        rct.xmin,
        rct.xmax,
        rct.ymax - NODE_DY,
        rct.ymax,
    };

    float color_header[4];

    /* Muted nodes get a mix of the background with the node color. */
    if (node.flag & NODE_MUTED) {
      UI_GetThemeColorBlend4f(TH_BACK, color_id, 0.1f, color_header);
    }
    else {
      UI_GetThemeColorBlend4f(TH_NODE, color_id, 0.4f, color_header);
    }

    UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
    UI_draw_roundbox_4fv(&rect, true, BASIS_RAD, color_header);
  }

  /* Show/hide icons. */
  float iconofs = rct.xmax - 0.35f * U.widget_unit;

  /* Preview. */
  if (node.typeinfo->flag & NODE_PREVIEW) {
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_MATERIAL,
                              iconofs,
                              rct.ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              0,
                              0,
                              "");
    UI_but_func_set(but, node_toggle_button_cb, &node, (void *)"NODE_OT_preview_toggle");
    /* XXX this does not work when node is activated and the operator called right afterwards,
     * since active ID is not updated yet (needs to process the notifier).
     * This can only work as visual indicator! */
    //      if (!(node.flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT)))
    //          UI_but_flag_enable(but, UI_BUT_DISABLED);
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  /* Group edit. */
  if (node.type == NODE_GROUP) {
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_NODETREE,
                              iconofs,
                              rct.ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              0,
                              0,
                              "");
    UI_but_func_set(but, node_toggle_button_cb, &node, (void *)"NODE_OT_group_edit");
    if (node.id) {
      UI_but_icon_indicator_number_set(but, ID_REAL_USERS(node.id));
    }
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  if (node.type == NODE_CUSTOM && node.typeinfo->ui_icon != ICON_NONE) {
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiDefIconBut(&block,
                 UI_BTYPE_BUT,
                 0,
                 node.typeinfo->ui_icon,
                 iconofs,
                 rct.ymax - NODE_DY,
                 iconbutw,
                 UI_UNIT_Y,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 "");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }
  if (node.type == GEO_NODE_VIEWER) {
    const bool is_active = &node == tree_draw_ctx.active_geometry_nodes_viewer;
    iconofs -= iconbutw;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT,
                              0,
                              is_active ? ICON_HIDE_OFF : ICON_HIDE_ON,
                              iconofs,
                              rct.ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              0,
                              0,
                              "");
    /* Selection implicitly activates the node. */
    const char *operator_idname = is_active ? "NODE_OT_deactivate_viewer" : "NODE_OT_select";
    UI_but_func_set(but, node_toggle_button_cb, &node, (void *)operator_idname);
    UI_block_emboss_set(&block, UI_EMBOSS);
  }

  node_add_error_message_button(tree_draw_ctx, node, block, rct, iconofs);

  /* Title. */
  if (node.flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* Collapse/expand icon. */
  {
    const int but_size = U.widget_unit * 0.8f;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);

    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_DOWNARROW_HLT,
                              rct.xmin + (NODE_MARGIN_X / 3),
                              rct.ymax - NODE_DY / 2.2f - but_size / 2,
                              but_size,
                              but_size,
                              nullptr,
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              "");

    UI_but_func_set(but, node_toggle_button_cb, &node, (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }

  char showname[128];
  nodeLabel(&ntree, &node, showname, sizeof(showname));

  uiBut *but = uiDefBut(&block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        int(rct.xmin + NODE_MARGIN_X + 0.4f),
                        int(rct.ymax - NODE_DY),
                        short(iconofs - rct.xmin - (18.0f * U.dpi_fac)),
                        short(NODE_DY),
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        "");
  if (node.flag & NODE_MUTED) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* Wire across the node when muted/disabled. */
  if (node.flag & NODE_MUTED) {
    node_draw_mute_line(C, v2d, snode, node);
  }

  /* Body. */
  const float outline_width = 1.0f;
  {
    /* Use warning color to indicate undefined types. */
    if (nodeTypeUndefined(&node)) {
      UI_GetThemeColorBlend4f(TH_REDALERT, TH_NODE, 0.4f, color);
    }
    /* Muted nodes get a mix of the background with the node color. */
    else if (node.flag & NODE_MUTED) {
      UI_GetThemeColorBlend4f(TH_BACK, TH_NODE, 0.2f, color);
    }
    else if (node.flag & NODE_CUSTOM_COLOR) {
      rgba_float_args_set(color, node.color[0], node.color[1], node.color[2], 1.0f);
    }
    else {
      UI_GetThemeColor4fv(TH_NODE, color);
    }

    /* Draw selected nodes fully opaque. */
    if (node.flag & SELECT) {
      color[3] = 1.0f;
    }

    /* Draw muted nodes slightly transparent so the wires inside are visible. */
    if (node.flag & NODE_MUTED) {
      color[3] -= 0.2f;
    }

    const rctf rect = {
        rct.xmin,
        rct.xmax,
        rct.ymin,
        rct.ymax - (NODE_DY + outline_width),
    };

    UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT);
    UI_draw_roundbox_4fv(&rect, true, BASIS_RAD, color);
  }

  /* Header underline. */
  {
    float color_underline[4];

    if (node.flag & NODE_MUTED) {
      UI_GetThemeColor4fv(TH_WIRE, color_underline);
      color_underline[3] = 1.0f;
    }
    else {
      UI_GetThemeColorBlend4f(TH_BACK, color_id, 0.2f, color_underline);
    }

    const rctf rect = {
        rct.xmin,
        rct.xmax,
        rct.ymax - (NODE_DY + outline_width),
        rct.ymax - NODE_DY,
    };

    UI_draw_roundbox_corner_set(UI_CNR_NONE);
    UI_draw_roundbox_4fv(&rect, true, 0.0f, color_underline);
  }

  /* Outline. */
  {
    const rctf rect = {
        rct.xmin - outline_width,
        rct.xmax + outline_width,
        rct.ymin - outline_width,
        rct.ymax + outline_width,
    };

    /* Color the outline according to active, selected, or undefined status. */
    float color_outline[4];

    if (node.flag & SELECT) {
      UI_GetThemeColor4fv((node.flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, color_outline);
    }
    else if (nodeTypeUndefined(&node)) {
      UI_GetThemeColor4fv(TH_REDALERT, color_outline);
    }
    else {
      UI_GetThemeColorBlendShade4fv(TH_BACK, TH_NODE, 0.4f, -20, color_outline);
    }

    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(&rect, false, BASIS_RAD + outline_width, color_outline);
  }

  float scale;
  UI_view2d_scale_get(&v2d, &scale, nullptr);

  /* Skip slow socket drawing if zoom is small. */
  if (scale > 0.2f) {
    node_draw_sockets(v2d, C, ntree, node, block, true, false);
  }

  /* Preview. */
  bNodeInstanceHash *previews =
      (bNodeInstanceHash *)CTX_data_pointer_get(&C, "node_previews").data;
  if (node.flag & NODE_PREVIEW && previews) {
    bNodePreview *preview = (bNodePreview *)BKE_node_instance_hash_lookup(previews, key);
    if (preview && (preview->xsize && preview->ysize)) {
      if (preview->rect && !BLI_rctf_is_empty(&node.runtime->prvr)) {
        node_draw_preview(preview, &node.runtime->prvr);
      }
    }
  }

  UI_block_end(&C, &block);
  UI_block_draw(&C, &block);
}

static void node_draw_hidden(const bContext &C,
                             TreeDrawContext &tree_draw_ctx,
                             const View2D &v2d,
                             const SpaceNode &snode,
                             bNodeTree &ntree,
                             bNode &node,
                             uiBlock &block)
{
  const rctf &rct = node.runtime->totr;
  float centy = BLI_rctf_cent_y(&rct);
  float hiddenrad = BLI_rctf_size_y(&rct) / 2.0f;

  float scale;
  UI_view2d_scale_get(&v2d, &scale, nullptr);

  const int color_id = node_get_colorid(tree_draw_ctx, node);

  node_draw_extra_info_panel(tree_draw_ctx, snode, node, block);

  /* Shadow. */
  node_draw_shadow(snode, node, hiddenrad, 1.0f);

  /* Wire across the node when muted/disabled. */
  if (node.flag & NODE_MUTED) {
    node_draw_mute_line(C, v2d, snode, node);
  }

  /* Body. */
  float color[4];
  {
    if (nodeTypeUndefined(&node)) {
      /* Use warning color to indicate undefined types. */
      UI_GetThemeColorBlend4f(TH_REDALERT, TH_NODE, 0.4f, color);
    }
    else if (node.flag & NODE_MUTED) {
      /* Muted nodes get a mix of the background with the node color. */
      UI_GetThemeColorBlendShade4fv(TH_BACK, color_id, 0.1f, 0, color);
    }
    else if (node.flag & NODE_CUSTOM_COLOR) {
      rgba_float_args_set(color, node.color[0], node.color[1], node.color[2], 1.0f);
    }
    else {
      UI_GetThemeColorBlend4f(TH_NODE, color_id, 0.4f, color);
    }

    /* Draw selected nodes fully opaque. */
    if (node.flag & SELECT) {
      color[3] = 1.0f;
    }

    /* Draw muted nodes slightly transparent so the wires inside are visible. */
    if (node.flag & NODE_MUTED) {
      color[3] -= 0.2f;
    }

    UI_draw_roundbox_4fv(&rct, true, hiddenrad, color);
  }

  /* Title. */
  if (node.flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* Collapse/expand icon. */
  {
    const int but_size = U.widget_unit * 1.0f;
    UI_block_emboss_set(&block, UI_EMBOSS_NONE);

    uiBut *but = uiDefIconBut(&block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_RIGHTARROW,
                              rct.xmin + (NODE_MARGIN_X / 3),
                              centy - but_size / 2,
                              but_size,
                              but_size,
                              nullptr,
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              "");

    UI_but_func_set(but, node_toggle_button_cb, &node, (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(&block, UI_EMBOSS);
  }

  char showname[128];
  nodeLabel(&ntree, &node, showname, sizeof(showname));

  uiBut *but = uiDefBut(&block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        round_fl_to_int(rct.xmin + NODE_MARGIN_X),
                        round_fl_to_int(centy - NODE_DY * 0.5f),
                        short(BLI_rctf_size_x(&rct) - ((18.0f + 12.0f) * U.dpi_fac)),
                        short(NODE_DY),
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        "");

  /* Outline. */
  {
    const float outline_width = 1.0f;
    const rctf rect = {
        rct.xmin - outline_width,
        rct.xmax + outline_width,
        rct.ymin - outline_width,
        rct.ymax + outline_width,
    };

    /* Color the outline according to active, selected, or undefined status. */
    float color_outline[4];

    if (node.flag & SELECT) {
      UI_GetThemeColor4fv((node.flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, color_outline);
    }
    else if (nodeTypeUndefined(&node)) {
      UI_GetThemeColor4fv(TH_REDALERT, color_outline);
    }
    else {
      UI_GetThemeColorBlendShade4fv(TH_BACK, TH_NODE, 0.4f, -20, color_outline);
    }

    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(&rect, false, hiddenrad, color_outline);
  }

  if (node.flag & NODE_MUTED) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* Scale widget thing. */
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformThemeColorShadeAlpha(TH_TEXT, -40, -180);
  float dx = 0.5f * U.widget_unit;
  const float dx2 = 0.15f * U.widget_unit * snode.runtime->aspect;
  const float dy = 0.2f * U.widget_unit;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct.xmax - dx, centy - dy);
  immVertex2f(pos, rct.xmax - dx, centy + dy);

  immVertex2f(pos, rct.xmax - dx - dx2, centy - dy);
  immVertex2f(pos, rct.xmax - dx - dx2, centy + dy);
  immEnd();

  immUniformThemeColorShadeAlpha(TH_TEXT, 0, -180);
  dx -= snode.runtime->aspect;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct.xmax - dx, centy - dy);
  immVertex2f(pos, rct.xmax - dx, centy + dy);

  immVertex2f(pos, rct.xmax - dx - dx2, centy - dy);
  immVertex2f(pos, rct.xmax - dx - dx2, centy + dy);
  immEnd();

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  node_draw_sockets(v2d, C, ntree, node, block, true, false);

  UI_block_end(&C, &block);
  UI_block_draw(&C, &block);
}

int node_get_resize_cursor(NodeResizeDirection directions)
{
  if (directions == 0) {
    return WM_CURSOR_DEFAULT;
  }
  if ((directions & ~(NODE_RESIZE_TOP | NODE_RESIZE_BOTTOM)) == 0) {
    return WM_CURSOR_Y_MOVE;
  }
  if ((directions & ~(NODE_RESIZE_RIGHT | NODE_RESIZE_LEFT)) == 0) {
    return WM_CURSOR_X_MOVE;
  }
  return WM_CURSOR_EDIT;
}

void node_set_cursor(wmWindow &win, SpaceNode &snode, const float2 &cursor)
{
  const bNodeTree *ntree = snode.edittree;
  if (ntree == nullptr) {
    WM_cursor_set(&win, WM_CURSOR_DEFAULT);
    return;
  }

  bNode *node;
  bNodeSocket *sock;
  int wmcursor = WM_CURSOR_DEFAULT;

  if (node_find_indicated_socket(
          snode, &node, &sock, cursor, (eNodeSocketInOut)(SOCK_IN | SOCK_OUT))) {
    WM_cursor_set(&win, WM_CURSOR_DEFAULT);
    return;
  }

  /* Check nodes front to back. */
  for (node = (bNode *)ntree->nodes.last; node; node = node->prev) {
    if (BLI_rctf_isect_pt(&node->runtime->totr, cursor[0], cursor[1])) {
      break; /* First hit on node stops. */
    }
  }
  if (node) {
    NodeResizeDirection dir = node_get_resize_direction(node, cursor[0], cursor[1]);
    wmcursor = node_get_resize_cursor(dir);
    /* We want to indicate that Frame nodes can be moved/selected on their borders. */
    if (node->type == NODE_FRAME && dir == NODE_RESIZE_NONE) {
      const rctf frame_inside = node_frame_rect_inside(*node);
      if (!BLI_rctf_isect_pt(&frame_inside, cursor[0], cursor[1])) {
        wmcursor = WM_CURSOR_NSEW_SCROLL;
      }
    }
  }

  WM_cursor_set(&win, wmcursor);
}

static void count_multi_input_socket_links(bNodeTree &ntree, SpaceNode &snode)
{
  Map<bNodeSocket *, int> counts;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (link->tosock->flag & SOCK_MULTI_INPUT) {
      int &count = counts.lookup_or_add(link->tosock, 0);
      count++;
    }
  }
  /* Count temporary links going into this socket. */
  if (snode.runtime->linkdrag) {
    for (const bNodeLink *link : snode.runtime->linkdrag->links) {
      if (link->tosock && (link->tosock->flag & SOCK_MULTI_INPUT)) {
        int &count = counts.lookup_or_add(link->tosock, 0);
        count++;
      }
    }
  }

  for (bNode *node : ntree.all_nodes()) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      if (socket->flag & SOCK_MULTI_INPUT) {
        socket->runtime->total_inputs = counts.lookup_default(socket, 0);
      }
    }
  }
}

/* XXX Does a bounding box update by iterating over all children.
 * Not ideal to do this in every draw call, but doing as transform callback doesn't work,
 * since the child node totr rects are not updated properly at that point. */
static void frame_node_prepare_for_draw(bNode &node, Span<bNode *> nodes)
{
  const float margin = 1.5f * U.widget_unit;
  NodeFrame *data = (NodeFrame *)node.storage;

  /* init rect from current frame size */
  rctf rect;
  node_to_updated_rect(node, rect);

  /* frame can be resized manually only if shrinking is disabled or no children are attached */
  data->flag |= NODE_FRAME_RESIZEABLE;
  /* for shrinking bbox, initialize the rect from first child node */
  bool bbinit = (data->flag & NODE_FRAME_SHRINK);
  /* fit bounding box to all children */
  for (const bNode *tnode : nodes) {
    if (tnode->parent != &node) {
      continue;
    }

    /* add margin to node rect */
    rctf noderect = tnode->runtime->totr;
    noderect.xmin -= margin;
    noderect.xmax += margin;
    noderect.ymin -= margin;
    noderect.ymax += margin;

    /* first child initializes frame */
    if (bbinit) {
      bbinit = false;
      rect = noderect;
      data->flag &= ~NODE_FRAME_RESIZEABLE;
    }
    else {
      BLI_rctf_union(&rect, &noderect);
    }
  }

  /* now adjust the frame size from view-space bounding box */
  const float2 offset = node_from_view(node, {rect.xmin, rect.ymax});
  node.offsetx = offset.x;
  node.offsety = offset.y;
  const float2 max = node_from_view(node, {rect.xmax, rect.ymin});
  node.width = max.x - node.offsetx;
  node.height = -max.y + node.offsety;

  node.runtime->totr = rect;
}

static void reroute_node_prepare_for_draw(bNode &node)
{
  /* get "global" coords */
  const float2 loc = node_to_view(node, float2(0));

  /* reroute node has exactly one input and one output, both in the same place */
  bNodeSocket *socket = (bNodeSocket *)node.outputs.first;
  socket->runtime->locx = loc.x;
  socket->runtime->locy = loc.y;

  socket = (bNodeSocket *)node.inputs.first;
  socket->runtime->locx = loc.x;
  socket->runtime->locy = loc.y;

  const float size = 8.0f;
  node.width = size * 2;
  node.runtime->totr.xmin = loc.x - size;
  node.runtime->totr.xmax = loc.x + size;
  node.runtime->totr.ymax = loc.y + size;
  node.runtime->totr.ymin = loc.y - size;
}

static void node_update_nodetree(const bContext &C,
                                 TreeDrawContext &tree_draw_ctx,
                                 bNodeTree &ntree,
                                 Span<bNode *> nodes,
                                 Span<uiBlock *> blocks)
{
  /* Make sure socket "used" tags are correct, for displaying value buttons. */
  SpaceNode *snode = CTX_wm_space_node(&C);

  count_multi_input_socket_links(ntree, *snode);

  for (const int i : nodes.index_range()) {
    bNode &node = *nodes[i];
    uiBlock &block = *blocks[i];
    if (node.type == NODE_FRAME) {
      /* Frame sizes are calculated after all other nodes have calculating their #totr. */
      continue;
    }

    if (node.type == NODE_REROUTE) {
      reroute_node_prepare_for_draw(node);
    }
    else {
      if (node.flag & NODE_HIDDEN) {
        node_update_hidden(node, block);
      }
      else {
        node_update_basis(C, tree_draw_ctx, ntree, node, block);
      }
    }
  }

  /* Now calculate the size of frame nodes, which can depend on the size of other nodes.
   * Update nodes in reverse, so children sizes get updated before parents. */
  for (int i = nodes.size() - 1; i >= 0; i--) {
    if (nodes[i]->type == NODE_FRAME) {
      frame_node_prepare_for_draw(*nodes[i], nodes);
    }
  }
}

static void frame_node_draw_label(TreeDrawContext &tree_draw_ctx,
                                  const bNodeTree &ntree,
                                  const bNode &node,
                                  const SpaceNode &snode)
{
  const float aspect = snode.runtime->aspect;
  /* XXX font id is crap design */
  const int fontid = UI_style_get()->widgetlabel.uifont_id;
  const NodeFrame *data = (const NodeFrame *)node.storage;
  const float font_size = data->label_size / aspect;

  char label[MAX_NAME];
  nodeLabel(&ntree, &node, label, sizeof(label));

  BLF_enable(fontid, BLF_ASPECT);
  BLF_aspect(fontid, aspect, aspect, 1.0f);
  /* clamp otherwise it can suck up a LOT of memory */
  BLF_size(fontid, MIN2(24.0f, font_size) * U.dpi_fac);

  /* title color */
  int color_id = node_get_colorid(tree_draw_ctx, node);
  uchar color[3];
  UI_GetThemeColorBlendShade3ubv(TH_TEXT, color_id, 0.4f, 10, color);
  BLF_color3ubv(fontid, color);

  const float margin = float(NODE_DY / 4);
  const float width = BLF_width(fontid, label, sizeof(label));
  const float ascender = BLF_ascender(fontid);
  const int label_height = ((margin / aspect) + (ascender * aspect));

  /* 'x' doesn't need aspect correction */
  const rctf &rct = node.runtime->totr;
  /* XXX a bit hacky, should use separate align values for x and y */
  float x = BLI_rctf_cent_x(&rct) - (0.5f * width);
  float y = rct.ymax - label_height;

  /* label */
  const bool has_label = node.label[0] != '\0';
  if (has_label) {
    BLF_position(fontid, x, y, 0);
    BLF_draw(fontid, label, sizeof(label));
  }

  /* draw text body */
  if (node.id) {
    const Text *text = (const Text *)node.id;
    const int line_height_max = BLF_height_max(fontid);
    const float line_spacing = (line_height_max * aspect);
    const float line_width = (BLI_rctf_size_x(&rct) - margin) / aspect;

    /* 'x' doesn't need aspect correction */
    x = rct.xmin + margin;
    y = rct.ymax - label_height - (has_label ? line_spacing : 0);

    /* early exit */
    int y_min = y + ((margin * 2) - (y - rct.ymin));

    BLF_enable(fontid, BLF_CLIPPING | BLF_WORD_WRAP);
    BLF_clipping(fontid,
                 rct.xmin,
                 /* round to avoid clipping half-way through a line */
                 y - (floorf(((y - rct.ymin) - (margin * 2)) / line_spacing) * line_spacing),
                 rct.xmin + line_width,
                 rct.ymax);

    BLF_wordwrap(fontid, line_width);

    LISTBASE_FOREACH (const TextLine *, line, &text->lines) {
      if (line->line[0]) {
        BLF_position(fontid, x, y, 0);
        ResultBLF info;
        BLF_draw_ex(fontid, line->line, line->len, &info);
        y -= line_spacing * info.lines;
      }
      else {
        y -= line_spacing;
      }
      if (y < y_min) {
        break;
      }
    }

    BLF_disable(fontid, BLF_CLIPPING | BLF_WORD_WRAP);
  }

  BLF_disable(fontid, BLF_ASPECT);
}

static void frame_node_draw(const bContext &C,
                            TreeDrawContext &tree_draw_ctx,
                            const ARegion &region,
                            const SpaceNode &snode,
                            bNodeTree &ntree,
                            bNode &node,
                            uiBlock &block)
{
  /* skip if out of view */
  if (BLI_rctf_isect(&node.runtime->totr, &region.v2d.cur, nullptr) == false) {
    UI_block_end(&C, &block);
    return;
  }

  float color[4];
  UI_GetThemeColor4fv(TH_NODE_FRAME, color);
  const float alpha = color[3];

  /* shadow */
  node_draw_shadow(snode, node, BASIS_RAD, alpha);

  /* body */
  if (node.flag & NODE_CUSTOM_COLOR) {
    rgba_float_args_set(color, node.color[0], node.color[1], node.color[2], alpha);
  }
  else {
    UI_GetThemeColor4fv(TH_NODE_FRAME, color);
  }

  const rctf &rct = node.runtime->totr;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(&rct, true, BASIS_RAD, color);

  /* outline active and selected emphasis */
  if (node.flag & SELECT) {
    if (node.flag & NODE_ACTIVE) {
      UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -40, color);
    }
    else {
      UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -40, color);
    }

    UI_draw_roundbox_aa(&rct, false, BASIS_RAD, color);
  }

  /* label and text */
  frame_node_draw_label(tree_draw_ctx, ntree, node, snode);

  node_draw_extra_info_panel(tree_draw_ctx, snode, node, block);

  UI_block_end(&C, &block);
  UI_block_draw(&C, &block);
}

static void reroute_node_draw(
    const bContext &C, ARegion &region, bNodeTree &ntree, bNode &node, uiBlock &block)
{
  /* skip if out of view */
  const rctf &rct = node.runtime->totr;
  if (rct.xmax < region.v2d.cur.xmin || rct.xmin > region.v2d.cur.xmax ||
      rct.ymax < region.v2d.cur.ymin || node.runtime->totr.ymin > region.v2d.cur.ymax) {
    UI_block_end(&C, &block);
    return;
  }

  if (node.label[0] != '\0') {
    /* draw title (node label) */
    char showname[128]; /* 128 used below */
    BLI_strncpy(showname, node.label, sizeof(showname));
    const short width = 512;
    const int x = BLI_rctf_cent_x(&node.runtime->totr) - (width / 2);
    const int y = node.runtime->totr.ymax;

    uiBut *label_but = uiDefBut(&block,
                                UI_BTYPE_LABEL,
                                0,
                                showname,
                                x,
                                y,
                                width,
                                short(NODE_DY),
                                nullptr,
                                0,
                                0,
                                0,
                                0,
                                nullptr);

    UI_but_drawflag_disable(label_but, UI_BUT_TEXT_LEFT);
  }

  /* only draw input socket. as they all are placed on the same position.
   * highlight also if node itself is selected, since we don't display the node body separately!
   */
  node_draw_sockets(region.v2d, C, ntree, node, block, false, node.flag & SELECT);

  UI_block_end(&C, &block);
  UI_block_draw(&C, &block);
}

static void node_draw(const bContext &C,
                      TreeDrawContext &tree_draw_ctx,
                      ARegion &region,
                      const SpaceNode &snode,
                      bNodeTree &ntree,
                      bNode &node,
                      uiBlock &block,
                      bNodeInstanceKey key)
{
  if (node.type == NODE_FRAME) {
    frame_node_draw(C, tree_draw_ctx, region, snode, ntree, node, block);
  }
  else if (node.type == NODE_REROUTE) {
    reroute_node_draw(C, region, ntree, node, block);
  }
  else {
    const View2D &v2d = region.v2d;
    if (node.flag & NODE_HIDDEN) {
      node_draw_hidden(C, tree_draw_ctx, v2d, snode, ntree, node, block);
    }
    else {
      node_draw_basis(C, tree_draw_ctx, v2d, snode, ntree, node, block, key);
    }
  }
}

#define USE_DRAW_TOT_UPDATE

static void node_draw_nodetree(const bContext &C,
                               TreeDrawContext &tree_draw_ctx,
                               ARegion &region,
                               SpaceNode &snode,
                               bNodeTree &ntree,
                               Span<bNode *> nodes,
                               Span<uiBlock *> blocks,
                               bNodeInstanceKey parent_key)
{
#ifdef USE_DRAW_TOT_UPDATE
  BLI_rctf_init_minmax(&region.v2d.tot);
#endif

  /* Draw background nodes, last nodes in front. */
  for (const int i : nodes.index_range()) {
#ifdef USE_DRAW_TOT_UPDATE
    /* Unrelated to background nodes, update the v2d->tot,
     * can be anywhere before we draw the scroll bars. */
    BLI_rctf_union(&region.v2d.tot, &nodes[i]->runtime->totr);
#endif

    if (!(nodes[i]->flag & NODE_BACKGROUND)) {
      continue;
    }

    const bNodeInstanceKey key = BKE_node_instance_key(parent_key, &ntree, nodes[i]);
    node_draw(C, tree_draw_ctx, region, snode, ntree, *nodes[i], *blocks[i], key);
  }

  /* Node lines. */
  GPU_blend(GPU_BLEND_ALPHA);
  nodelink_batch_start(snode);

  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (!nodeLinkIsHidden(link) && !nodeLinkIsSelected(link)) {
      node_draw_link(C, region.v2d, snode, *link, false);
    }
  }

  /* Draw selected node links after the unselected ones, so they are shown on top. */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
    if (!nodeLinkIsHidden(link) && nodeLinkIsSelected(link)) {
      node_draw_link(C, region.v2d, snode, *link, true);
    }
  }

  nodelink_batch_end(snode);
  GPU_blend(GPU_BLEND_NONE);

  /* Draw foreground nodes, last nodes in front. */
  for (const int i : nodes.index_range()) {
    if (nodes[i]->flag & NODE_BACKGROUND) {
      continue;
    }

    const bNodeInstanceKey key = BKE_node_instance_key(parent_key, &ntree, nodes[i]);
    node_draw(C, tree_draw_ctx, region, snode, ntree, *nodes[i], *blocks[i], key);
  }
}

/* Draw the breadcrumb on the top of the editor. */
static void draw_tree_path(const bContext &C, ARegion &region)
{
  GPU_matrix_push_projection();
  wmOrtho2_region_pixelspace(&region);

  const rcti *rect = ED_region_visible_rect(&region);

  const uiStyle *style = UI_style_get_dpi();
  const float padding_x = 16 * UI_DPI_FAC;
  const int x = rect->xmin + padding_x;
  const int y = region.winy - UI_UNIT_Y * 0.6f;
  const int width = BLI_rcti_size_x(rect) - 2 * padding_x;

  uiBlock *block = UI_block_begin(&C, &region, __func__, UI_EMBOSS_NONE);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, x, y, width, 1, 0, style);

  const Vector<ui::ContextPathItem> context_path = ed::space_node::context_path_for_space_node(C);
  ui::template_breadcrumbs(*layout, context_path);

  UI_block_layout_resolve(block, nullptr, nullptr);
  UI_block_end(&C, block);
  UI_block_draw(&C, block);

  GPU_matrix_pop_projection();
}

static void snode_setup_v2d(SpaceNode &snode, ARegion &region, const float2 &center)
{
  View2D &v2d = region.v2d;

  /* Shift view to node tree center. */
  UI_view2d_center_set(&v2d, center[0], center[1]);
  UI_view2d_view_ortho(&v2d);

  /* Aspect + font, set each time. */
  snode.runtime->aspect = BLI_rctf_size_x(&v2d.cur) / float(region.winx);
  // XXX snode->curfont = uiSetCurFont_ext(snode->aspect);
}

/* Similar to is_compositor_enabled() in draw_manager.c but checks all 3D views. */
static bool realtime_compositor_is_in_use(const bContext &context)
{
  if (!U.experimental.use_realtime_compositor) {
    return false;
  }

  const Scene *scene = CTX_data_scene(&context);
  if (!scene->use_nodes) {
    return false;
  }

  if (!scene->nodetree) {
    return false;
  }

  const Main *main = CTX_data_main(&context);
  LISTBASE_FOREACH (bScreen *, screen, &main->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
        if (space->spacetype != SPACE_VIEW3D) {
          continue;
        }

        const View3D &view_3d = *reinterpret_cast<const View3D *>(space);

        if (view_3d.shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
          continue;
        }

        if (!(view_3d.shading.type >= OB_MATERIAL)) {
          continue;
        }

        return true;
      }
    }
  }

  return false;
}

static void draw_nodetree(const bContext &C,
                          ARegion &region,
                          bNodeTree &ntree,
                          bNodeInstanceKey parent_key)
{
  SpaceNode *snode = CTX_wm_space_node(&C);
  ntree.ensure_topology_cache();

  const Span<bNode *> nodes = ntree.all_nodes();

  Array<uiBlock *> blocks = node_uiblocks_init(C, nodes);

  TreeDrawContext tree_draw_ctx;
  if (ntree.type == NTREE_GEOMETRY) {
    tree_draw_ctx.geo_tree_log = geo_log::GeoModifierLog::get_tree_log_for_node_editor(*snode);
    if (tree_draw_ctx.geo_tree_log != nullptr) {
      tree_draw_ctx.geo_tree_log->ensure_node_warnings();
      tree_draw_ctx.geo_tree_log->ensure_node_run_time();
    }
    const WorkSpace *workspace = CTX_wm_workspace(&C);
    tree_draw_ctx.active_geometry_nodes_viewer = viewer_path::find_geometry_nodes_viewer(
        workspace->viewer_path, *snode);
  }
  else if (ntree.type == NTREE_COMPOSIT) {
    tree_draw_ctx.used_by_realtime_compositor = realtime_compositor_is_in_use(C);
  }

  node_update_nodetree(C, tree_draw_ctx, ntree, nodes, blocks);
  node_draw_nodetree(C, tree_draw_ctx, region, *snode, ntree, nodes, blocks, parent_key);
}

/**
 * Make the background slightly brighter to indicate that users are inside a node-group.
 */
static void draw_background_color(const SpaceNode &snode)
{
  const int max_tree_length = 3;
  const float bright_factor = 0.25f;

  /* We ignore the first element of the path since it is the top-most tree and it doesn't need to
   * be brighter. We also set a cap to how many levels we want to set apart, to avoid the
   * background from getting too bright. */
  const int clamped_tree_path_length = BLI_listbase_count_at_most(&snode.treepath,
                                                                  max_tree_length);
  const int depth = max_ii(0, clamped_tree_path_length - 1);

  float color[3];
  UI_GetThemeColor3fv(TH_BACK, color);
  mul_v3_fl(color, 1.0f + bright_factor * depth);
  GPU_clear_color(color[0], color[1], color[2], 1.0);
}

void node_draw_space(const bContext &C, ARegion &region)
{
  wmWindow *win = CTX_wm_window(&C);
  SpaceNode &snode = *CTX_wm_space_node(&C);
  View2D &v2d = region.v2d;

  /* Setup off-screen buffers. */
  GPUViewport *viewport = WM_draw_region_get_viewport(&region);

  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  UI_view2d_view_ortho(&v2d);
  draw_background_color(snode);
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_scissor_test(true);

  /* XXX `snode->runtime->cursor` set in coordinate-space for placing new nodes,
   * used for drawing noodles too. */
  UI_view2d_region_to_view(&region.v2d,
                           win->eventstate->xy[0] - region.winrct.xmin,
                           win->eventstate->xy[1] - region.winrct.ymin,
                           &snode.runtime->cursor[0],
                           &snode.runtime->cursor[1]);
  snode.runtime->cursor[0] /= UI_DPI_FAC;
  snode.runtime->cursor[1] /= UI_DPI_FAC;

  ED_region_draw_cb_draw(&C, &region, REGION_DRAW_PRE_VIEW);

  /* Only set once. */
  GPU_blend(GPU_BLEND_ALPHA);

  /* Nodes. */
  snode_set_context(C);

  const int grid_levels = UI_GetThemeValueType(TH_NODE_GRID_LEVELS, SPACE_NODE);
  UI_view2d_dot_grid_draw(&v2d, TH_GRID, NODE_GRID_STEP_SIZE, grid_levels);

  /* Draw parent node trees. */
  if (snode.treepath.last) {
    bNodeTreePath *path = (bNodeTreePath *)snode.treepath.last;

    /* Update tree path name (drawn in the bottom left). */
    ID *name_id = (path->nodetree && path->nodetree != snode.nodetree) ? &path->nodetree->id :
                                                                         snode.id;

    if (name_id && UNLIKELY(!STREQ(path->display_name, name_id->name + 2))) {
      BLI_strncpy(path->display_name, name_id->name + 2, sizeof(path->display_name));
    }

    /* Current View2D center, will be set temporarily for parent node trees. */
    float2 center;
    UI_view2d_center_get(&v2d, &center.x, &center.y);

    /* Store new view center in path and current edit tree. */
    copy_v2_v2(path->view_center, center);
    if (snode.edittree) {
      copy_v2_v2(snode.edittree->view_center, center);
    }

    /* Top-level edit tree. */
    bNodeTree *ntree = path->nodetree;
    if (ntree) {
      snode_setup_v2d(snode, region, center);

      /* Backdrop. */
      draw_nodespace_back_pix(C, region, snode, path->parent_key);

      {
        float original_proj[4][4];
        GPU_matrix_projection_get(original_proj);

        GPU_matrix_push();
        GPU_matrix_identity_set();

        wmOrtho2_pixelspace(region.winx, region.winy);

        WM_gizmomap_draw(region.gizmo_map, &C, WM_GIZMOMAP_DRAWSTEP_2D);

        GPU_matrix_pop();
        GPU_matrix_projection_set(original_proj);
      }

      draw_nodetree(C, region, *ntree, path->parent_key);
    }

    /* Temporary links. */
    GPU_blend(GPU_BLEND_ALPHA);
    GPU_line_smooth(true);
    if (snode.runtime->linkdrag) {
      for (const bNodeLink *link : snode.runtime->linkdrag->links) {
        node_draw_link_dragged(C, v2d, snode, *link);
      }
    }
    GPU_line_smooth(false);
    GPU_blend(GPU_BLEND_NONE);

    if (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS && snode.flag & SNODE_SHOW_GPENCIL) {
      /* Draw grease-pencil annotations. */
      ED_annotation_draw_view2d(&C, true);
    }
  }
  else {

    /* Backdrop. */
    draw_nodespace_back_pix(C, region, snode, NODE_INSTANCE_KEY_NONE);
  }

  ED_region_draw_cb_draw(&C, &region, REGION_DRAW_POST_VIEW);

  /* Reset view matrix. */
  UI_view2d_view_restore(&C);

  if (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS) {
    if (snode.flag & SNODE_SHOW_GPENCIL && snode.treepath.last) {
      /* Draw grease-pencil (screen strokes, and also paint-buffer). */
      ED_annotation_draw_view2d(&C, false);
    }

    /* Draw context path. */
    if (snode.overlay.flag & SN_OVERLAY_SHOW_PATH && snode.edittree) {
      draw_tree_path(C, region);
    }
  }

  /* Scrollers. */
  UI_view2d_scrollers_draw(&v2d, nullptr);
}

}  // namespace blender::ed::space_node
