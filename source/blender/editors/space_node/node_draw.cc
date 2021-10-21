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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spnode
 * \brief higher level node drawing for the node editor.
 */

#include "MEM_guardedalloc.h"

#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_geometry_set.hh"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"

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
#include "ED_space_api.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "NOD_geometry_nodes_eval_log.hh"
#include "NOD_node_declaration.hh"

#include "FN_field_cpp_type.hh"

#include "node_intern.h" /* own include */

#ifdef WITH_COMPOSITOR
#  include "COM_compositor.h"
#endif

using blender::Map;
using blender::Set;
using blender::Span;
using blender::Vector;
using blender::VectorSet;
using blender::fn::CPPType;
using blender::fn::FieldCPPType;
using blender::fn::FieldInput;
using blender::fn::GField;
using blender::fn::GPointer;
namespace geo_log = blender::nodes::geometry_nodes_eval_log;

extern "C" {
/* XXX interface.h */
extern void ui_draw_dropshadow(
    const rctf *rct, float radius, float aspect, float alpha, int select);
}

float ED_node_grid_size(void)
{
  return U.widget_unit;
}

void ED_node_tree_update(const bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode) {
    snode_set_context(C);

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

void ED_node_tag_update_nodetree(Main *bmain, bNodeTree *ntree, bNode *node)
{
  if (!ntree) {
    return;
  }

  bool do_tag_update = true;
  if (node != nullptr) {
    if (!node_connected_to_output(bmain, ntree, node)) {
      do_tag_update = false;
    }
  }

  /* Look through all datablocks to support groups. */
  if (do_tag_update) {
    FOREACH_NODETREE_BEGIN (bmain, tntree, id) {
      /* Check if nodetree uses the group. */
      if (ntreeHasTree(tntree, ntree)) {
        ED_node_tag_update_id(id);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (ntree->type == NTREE_TEXTURE) {
    ntreeTexCheckCyclics(ntree);
  }
}

static bool compare_nodes(const bNode *a, const bNode *b)
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
      return true;
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
      return false;
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
    return false;
  }
  if (!(a->flag & NODE_BACKGROUND) && (b->flag & NODE_BACKGROUND)) {
    return true;
  }

  /* One has a higher selection state (active > selected > nothing). */
  if (!b_active && a_active) {
    return true;
  }
  if (!b_select && (a_active || a_select)) {
    return true;
  }

  return false;
}

/**
 * Sort nodes by selection: unselected nodes first, then selected,
 * then the active node at the very end. Relative order is kept intact.
 */
void ED_node_sort(bNodeTree *ntree)
{
  /* Merge sort is the algorithm of choice here. */
  int totnodes = BLI_listbase_count(&ntree->nodes);

  int k = 1;
  while (k < totnodes) {
    bNode *first_a = (bNode *)ntree->nodes.first;
    bNode *first_b = first_a;

    do {
      /* Set up first_b pointer. */
      for (int b = 0; b < k && first_b; b++) {
        first_b = first_b->next;
      }
      /* All batches merged? */
      if (first_b == nullptr) {
        break;
      }

      /* Merge batches. */
      bNode *node_a = first_a;
      bNode *node_b = first_b;
      int a = 0;
      int b = 0;
      while (a < k && b < k && node_b) {
        if (compare_nodes(node_a, node_b) == 0) {
          node_a = node_a->next;
          a++;
        }
        else {
          bNode *tmp = node_b;
          node_b = node_b->next;
          b++;
          BLI_remlink(&ntree->nodes, tmp);
          BLI_insertlinkbefore(&ntree->nodes, node_a, tmp);
        }
      }

      /* Set up first pointers for next batch. */
      first_b = node_b;
      for (; b < k; b++) {
        /* All nodes sorted? */
        if (first_b == nullptr) {
          break;
        }
        first_b = first_b->next;
      }
      first_a = first_b;
    } while (first_b);

    k = k << 1;
  }
}

static void node_uiblocks_init(const bContext *C, bNodeTree *ntree)
{
  /* Add node uiBlocks in drawing order - prevents events going to overlapping nodes. */

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    /* ui block */
    char uiblockstr[32];
    BLI_snprintf(uiblockstr, sizeof(uiblockstr), "node buttons %p", (void *)node);
    node->block = UI_block_begin(C, CTX_wm_region(C), uiblockstr, UI_EMBOSS);

    /* this cancels events for background nodes */
    UI_block_flag_enable(node->block, UI_BLOCK_CLIP_EVENTS);
  }
}

void node_to_view(const bNode *node, float x, float y, float *rx, float *ry)
{
  nodeToView(node, x, y, rx, ry);
  *rx *= UI_DPI_FAC;
  *ry *= UI_DPI_FAC;
}

void node_to_updated_rect(const bNode *node, rctf *r_rect)
{
  node_to_view(node, node->offsetx, node->offsety, &r_rect->xmin, &r_rect->ymax);
  node_to_view(node,
               node->offsetx + node->width,
               node->offsety - node->height,
               &r_rect->xmax,
               &r_rect->ymin);
}

void node_from_view(const bNode *node, float x, float y, float *rx, float *ry)
{
  x /= UI_DPI_FAC;
  y /= UI_DPI_FAC;
  nodeFromView(node, x, y, rx, ry);
}

/**
 * Based on settings and sockets in node, set drawing rect info.
 */
static void node_update_basis(const bContext *C, bNodeTree *ntree, bNode *node)
{
  PointerRNA nodeptr;
  RNA_pointer_create(&ntree->id, &RNA_Node, node, &nodeptr);

  /* Get "global" coordinates. */
  float locx, locy;
  node_to_view(node, 0.0f, 0.0f, &locx, &locy);
  /* Round the node origin because text contents are always pixel-aligned. */
  locx = round(locx);
  locy = round(locy);

  int dy = locy;

  /* Header. */
  dy -= NODE_DY;

  /* Little bit of space in top. */
  if (node->outputs.first) {
    dy -= NODE_DYS / 2;
  }

  /* Output sockets. */
  bool add_output_space = false;

  int buty;
  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->outputs) {
    if (nodeSocketIsHidden(nsock)) {
      continue;
    }

    PointerRNA sockptr;
    RNA_pointer_create(&ntree->id, &RNA_NodeSocket, nsock, &sockptr);

    uiLayout *layout = UI_block_layout(node->block,
                                       UI_LAYOUT_VERTICAL,
                                       UI_LAYOUT_PANEL,
                                       locx + NODE_DYS,
                                       dy,
                                       NODE_WIDTH(node) - NODE_DY,
                                       NODE_DY,
                                       0,
                                       UI_style_get_dpi());

    if (node->flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    /* Context pointers for current node and socket. */
    uiLayoutSetContextPointer(layout, "node", &nodeptr);
    uiLayoutSetContextPointer(layout, "socket", &sockptr);

    /* Align output buttons to the right. */
    uiLayout *row = uiLayoutRow(layout, true);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
    const char *socket_label = nodeSocketLabel(nsock);
    nsock->typeinfo->draw((bContext *)C, row, &sockptr, &nodeptr, IFACE_(socket_label));

    UI_block_align_end(node->block);
    UI_block_layout_resolve(node->block, nullptr, &buty);

    /* Ensure minimum socket height in case layout is empty. */
    buty = min_ii(buty, dy - NODE_DY);

    /* Round the socket location to stop it from jiggling. */
    nsock->locx = round(locx + NODE_WIDTH(node));
    nsock->locy = round(0.5f * (dy + buty));

    dy = buty;
    if (nsock->next) {
      dy -= NODE_SOCKDY;
    }

    add_output_space = true;
  }

  if (add_output_space) {
    dy -= NODE_DY / 4;
  }

  node->prvr.xmin = locx + NODE_DYS;
  node->prvr.xmax = locx + NODE_WIDTH(node) - NODE_DYS;

  /* preview rect? */
  if (node->flag & NODE_PREVIEW) {
    float aspect = 1.0f;

    if (node->preview_xsize && node->preview_ysize) {
      aspect = (float)node->preview_ysize / (float)node->preview_xsize;
    }

    dy -= NODE_DYS / 2;
    node->prvr.ymax = dy;

    if (aspect <= 1.0f) {
      node->prvr.ymin = dy - aspect * (NODE_WIDTH(node) - NODE_DY);
    }
    else {
      /* Width correction of image. XXX huh? (ton) */
      float dx = (NODE_WIDTH(node) - NODE_DYS) - (NODE_WIDTH(node) - NODE_DYS) / aspect;

      node->prvr.ymin = dy - (NODE_WIDTH(node) - NODE_DY);

      node->prvr.xmin += 0.5f * dx;
      node->prvr.xmax -= 0.5f * dx;
    }

    dy = node->prvr.ymin - NODE_DYS / 2;

    /* Make sure that maximums are bigger or equal to minimums. */
    if (node->prvr.xmax < node->prvr.xmin) {
      SWAP(float, node->prvr.xmax, node->prvr.xmin);
    }
    if (node->prvr.ymax < node->prvr.ymin) {
      SWAP(float, node->prvr.ymax, node->prvr.ymin);
    }
  }

  /* Buttons rect? */
  if (node->typeinfo->draw_buttons && (node->flag & NODE_OPTIONS)) {
    dy -= NODE_DYS / 2;

    /* Set this for `uifunc()` that don't use layout engine yet. */
    node->butr.xmin = 0;
    node->butr.xmax = NODE_WIDTH(node) - 2 * NODE_DYS;
    node->butr.ymin = 0;
    node->butr.ymax = 0;

    uiLayout *layout = UI_block_layout(node->block,
                                       UI_LAYOUT_VERTICAL,
                                       UI_LAYOUT_PANEL,
                                       locx + NODE_DYS,
                                       dy,
                                       node->butr.xmax,
                                       0,
                                       0,
                                       UI_style_get_dpi());

    if (node->flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    uiLayoutSetContextPointer(layout, "node", &nodeptr);

    node->typeinfo->draw_buttons(layout, (bContext *)C, &nodeptr);

    UI_block_align_end(node->block);
    UI_block_layout_resolve(node->block, nullptr, &buty);

    dy = buty - NODE_DYS / 2;
  }

  /* Input sockets. */
  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->inputs) {
    if (nodeSocketIsHidden(nsock)) {
      continue;
    }

    PointerRNA sockptr;
    RNA_pointer_create(&ntree->id, &RNA_NodeSocket, nsock, &sockptr);

    /* Add the half the height of a multi-input socket to cursor Y
     * to account for the increased height of the taller sockets. */
    float multi_input_socket_offset = 0.0f;
    if (nsock->flag & SOCK_MULTI_INPUT) {
      if (nsock->total_inputs > 2) {
        multi_input_socket_offset = (nsock->total_inputs - 2) * NODE_MULTI_INPUT_LINK_GAP;
      }
    }
    dy -= multi_input_socket_offset * 0.5f;

    uiLayout *layout = UI_block_layout(node->block,
                                       UI_LAYOUT_VERTICAL,
                                       UI_LAYOUT_PANEL,
                                       locx + NODE_DYS,
                                       dy,
                                       NODE_WIDTH(node) - NODE_DY,
                                       NODE_DY,
                                       0,
                                       UI_style_get_dpi());

    if (node->flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    /* Context pointers for current node and socket. */
    uiLayoutSetContextPointer(layout, "node", &nodeptr);
    uiLayoutSetContextPointer(layout, "socket", &sockptr);

    uiLayout *row = uiLayoutRow(layout, true);

    const char *socket_label = nodeSocketLabel(nsock);
    nsock->typeinfo->draw((bContext *)C, row, &sockptr, &nodeptr, IFACE_(socket_label));

    UI_block_align_end(node->block);
    UI_block_layout_resolve(node->block, nullptr, &buty);

    /* Ensure minimum socket height in case layout is empty. */
    buty = min_ii(buty, dy - NODE_DY);

    nsock->locx = locx;
    /* Round the socket vertical position to stop it from jiggling. */
    nsock->locy = round(0.5f * (dy + buty));

    dy = buty - multi_input_socket_offset * 0.5;
    if (nsock->next) {
      dy -= NODE_SOCKDY;
    }
  }

  /* Little bit of space in end. */
  if (node->inputs.first || (node->flag & (NODE_OPTIONS | NODE_PREVIEW)) == 0) {
    dy -= NODE_DYS / 2;
  }

  node->totr.xmin = locx;
  node->totr.xmax = locx + NODE_WIDTH(node);
  node->totr.ymax = locy;
  node->totr.ymin = min_ff(dy, locy - 2 * NODE_DY);

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side. */
  UI_block_bounds_set_explicit(node->block,
                               node->totr.xmin - NODE_SOCKSIZE,
                               node->totr.ymin,
                               node->totr.xmax + NODE_SOCKSIZE,
                               node->totr.ymax);
}

/**
 * Based on settings in node, sets drawing rect info.
 */
static void node_update_hidden(bNode *node)
{
  int totin = 0, totout = 0;

  /* Get "global" coords. */
  float locx, locy;
  node_to_view(node, 0.0f, 0.0f, &locx, &locy);
  /* Round the node origin because text contents are always pixel-aligned. */
  locx = round(locx);
  locy = round(locy);

  /* Calculate minimal radius. */
  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->inputs) {
    if (!nodeSocketIsHidden(nsock)) {
      totin++;
    }
  }
  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->outputs) {
    if (!nodeSocketIsHidden(nsock)) {
      totout++;
    }
  }

  float hiddenrad = HIDDEN_RAD;
  float tot = MAX2(totin, totout);
  if (tot > 4) {
    hiddenrad += 5.0f * (float)(tot - 4);
  }

  node->totr.xmin = locx;
  node->totr.xmax = locx + max_ff(NODE_WIDTH(node), 2 * hiddenrad);
  node->totr.ymax = locy + (hiddenrad - 0.5f * NODE_DY);
  node->totr.ymin = node->totr.ymax - 2 * hiddenrad;

  /* Output sockets. */
  float rad = (float)M_PI / (1.0f + (float)totout);
  float drad = rad;

  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->outputs) {
    if (!nodeSocketIsHidden(nsock)) {
      /* Round the socket location to stop it from jiggling. */
      nsock->locx = round(node->totr.xmax - hiddenrad + sinf(rad) * hiddenrad);
      nsock->locy = round(node->totr.ymin + hiddenrad + cosf(rad) * hiddenrad);
      rad += drad;
    }
  }

  /* Input sockets. */
  rad = drad = -(float)M_PI / (1.0f + (float)totin);

  LISTBASE_FOREACH (bNodeSocket *, nsock, &node->inputs) {
    if (!nodeSocketIsHidden(nsock)) {
      /* Round the socket location to stop it from jiggling. */
      nsock->locx = round(node->totr.xmin + hiddenrad + sinf(rad) * hiddenrad);
      nsock->locy = round(node->totr.ymin + hiddenrad + cosf(rad) * hiddenrad);
      rad += drad;
    }
  }

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side. */
  UI_block_bounds_set_explicit(node->block,
                               node->totr.xmin - NODE_SOCKSIZE,
                               node->totr.ymin,
                               node->totr.xmax + NODE_SOCKSIZE,
                               node->totr.ymax);
}

void node_update_default(const bContext *C, bNodeTree *ntree, bNode *node)
{
  if (node->flag & NODE_HIDDEN) {
    node_update_hidden(node);
  }
  else {
    node_update_basis(C, ntree, node);
  }
}

int node_select_area_default(bNode *node, int x, int y)
{
  return BLI_rctf_isect_pt(&node->totr, x, y);
}

int node_tweak_area_default(bNode *node, int x, int y)
{
  return BLI_rctf_isect_pt(&node->totr, x, y);
}

int node_get_colorid(bNode *node)
{
  switch (node->typeinfo->nclass) {
    case NODE_CLASS_INPUT:
      return TH_NODE_INPUT;
    case NODE_CLASS_OUTPUT:
      return (node->flag & NODE_DO_OUTPUT) ? TH_NODE_OUTPUT : TH_NODE;
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

static void node_draw_mute_line(const View2D *v2d, const SpaceNode *snode, const bNode *node)
{
  GPU_blend(GPU_BLEND_ALPHA);

  LISTBASE_FOREACH (const bNodeLink *, link, &node->internal_links) {
    node_draw_link_bezier(v2d, snode, link, TH_REDALERT, TH_REDALERT, -1);
  }

  GPU_blend(GPU_BLEND_NONE);
}

static void node_socket_draw(const bNodeSocket *sock,
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
  switch (sock->display_shape) {
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

  if (ELEM(sock->display_shape,
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
                                         const int locx,
                                         const int locy)
{
  const float outline_width = 1.0f;
  /* UI_draw_roundbox draws the outline on the outer side, so compensate for the outline width. */
  const rctf rect = {
      locx - width + outline_width * 0.5f,
      locx + width - outline_width * 0.5f,
      locy - height + outline_width * 0.5f,
      locy + height - outline_width * 0.5f,
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
    UI_GetThemeColor4fv(TH_TEXT_HI, r_outline_color);
    r_outline_color[3] = 0.9f;
  }
  else {
    copy_v4_fl(r_outline_color, 0.0f);
    r_outline_color[3] = 0.6f;
  }

  /* Until there is a better place for per socket color,
   * the outline color for virtual sockets is set  here. */
  if (socket_type == SOCK_CUSTOM) {
    copy_v4_v4(r_outline_color, virtual_node_socket_outline_color);
  }
}

/* Usual convention here would be node_socket_get_color(), but that's already used (for setting a
 * color property socket). */
void node_socket_color_get(
    bContext *C, bNodeTree *ntree, PointerRNA *node_ptr, bNodeSocket *sock, float r_color[4])
{
  PointerRNA ptr;
  BLI_assert(RNA_struct_is_a(node_ptr->type, &RNA_Node));
  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);

  sock->typeinfo->draw_color(C, &ptr, node_ptr, r_color);

  bNode *node = (bNode *)node_ptr->data;
  if (node->flag & NODE_MUTED) {
    r_color[3] *= 0.25f;
  }
}

struct SocketTooltipData {
  bNodeTree *ntree;
  bNode *node;
  bNodeSocket *socket;
};

static void create_inspection_string_for_generic_value(const geo_log::GenericValueLog &value_log,
                                                       std::stringstream &ss)
{
  auto id_to_inspection_string = [&](ID *id, short idcode) {
    ss << (id ? id->name + 2 : TIP_("None")) << " (" << BKE_idtype_idcode_to_name(idcode) << ")";
  };

  const GPointer value = value_log.value();
  const CPPType &type = *value.type();
  if (const FieldCPPType *field_type = dynamic_cast<const FieldCPPType *>(&type)) {
    const CPPType &base_type = field_type->field_type();
    BUFFER_FOR_CPP_TYPE_VALUE(base_type, buffer);
    const GField &field = field_type->get_gfield(value.get());
    if (field.node().depends_on_input()) {
      if (base_type.is<int>()) {
        ss << TIP_("Integer Field");
      }
      else if (base_type.is<float>()) {
        ss << TIP_("Float Field");
      }
      else if (base_type.is<blender::float3>()) {
        ss << TIP_("Vector Field");
      }
      else if (base_type.is<bool>()) {
        ss << TIP_("Boolean Field");
      }
      else if (base_type.is<std::string>()) {
        ss << TIP_("String Field");
      }
      ss << TIP_(" based on:\n");

      /* Use vector set to deduplicate inputs. */
      VectorSet<std::reference_wrapper<const FieldInput>> field_inputs;
      field.node().foreach_field_input(
          [&](const FieldInput &field_input) { field_inputs.add(field_input); });
      for (const FieldInput &field_input : field_inputs) {
        ss << "\u2022 " << field_input.socket_inspection_name();
        if (field_input != field_inputs.as_span().last().get()) {
          ss << ".\n";
        }
      }
    }
    else {
      blender::fn::evaluate_constant_field(field, buffer);
      if (base_type.is<int>()) {
        ss << *(int *)buffer << TIP_(" (Integer)");
      }
      else if (base_type.is<float>()) {
        ss << *(float *)buffer << TIP_(" (Float)");
      }
      else if (base_type.is<blender::float3>()) {
        ss << *(blender::float3 *)buffer << TIP_(" (Vector)");
      }
      else if (base_type.is<bool>()) {
        ss << ((*(bool *)buffer) ? TIP_("True") : TIP_("False")) << TIP_(" (Boolean)");
      }
      else if (base_type.is<std::string>()) {
        ss << *(std::string *)buffer << TIP_(" (String)");
      }
      base_type.destruct(buffer);
    }
  }
  else if (type.is<Object *>()) {
    id_to_inspection_string((ID *)*value.get<Object *>(), ID_OB);
  }
  else if (type.is<Material *>()) {
    id_to_inspection_string((ID *)*value.get<Material *>(), ID_MA);
  }
  else if (type.is<Tex *>()) {
    id_to_inspection_string((ID *)*value.get<Tex *>(), ID_TE);
  }
  else if (type.is<Image *>()) {
    id_to_inspection_string((ID *)*value.get<Image *>(), ID_IM);
  }
  else if (type.is<Collection *>()) {
    id_to_inspection_string((ID *)*value.get<Collection *>(), ID_GR);
  }
}

static void create_inspection_string_for_geometry(const geo_log::GeometryValueLog &value_log,
                                                  std::stringstream &ss)
{
  Span<GeometryComponentType> component_types = value_log.component_types();
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
        const geo_log::GeometryValueLog::MeshInfo &mesh_info = *value_log.mesh_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Mesh: %s vertices, %s edges, %s faces"),
                     to_string(mesh_info.tot_verts).c_str(),
                     to_string(mesh_info.tot_edges).c_str(),
                     to_string(mesh_info.tot_faces).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        const geo_log::GeometryValueLog::PointCloudInfo &pointcloud_info =
            *value_log.pointcloud_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Point Cloud: %s points"),
                     to_string(pointcloud_info.tot_points).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_CURVE: {
        const geo_log::GeometryValueLog::CurveInfo &curve_info = *value_log.curve_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Curve: %s splines"),
                     to_string(curve_info.tot_splines).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_INSTANCES: {
        const geo_log::GeometryValueLog::InstancesInfo &instances_info = *value_log.instances_info;
        char line[256];
        BLI_snprintf(line,
                     sizeof(line),
                     TIP_("\u2022 Instances: %s"),
                     to_string(instances_info.tot_instances).c_str());
        ss << line << line_end;
        break;
      }
      case GEO_COMPONENT_TYPE_VOLUME: {
        ss << TIP_("\u2022 Volume") << line_end;
        break;
      }
    }
  }
}

static std::optional<std::string> create_socket_inspection_string(bContext *C,
                                                                  bNodeTree &UNUSED(ntree),
                                                                  bNode &node,
                                                                  bNodeSocket &socket)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  const geo_log::SocketLog *socket_log = geo_log::ModifierLog::find_socket_by_node_editor_context(
      *snode, node, socket);
  if (socket_log == nullptr) {
    return {};
  }
  const geo_log::ValueLog *value_log = socket_log->value();
  if (value_log == nullptr) {
    return {};
  }

  std::stringstream ss;
  if (const geo_log::GenericValueLog *generic_value_log =
          dynamic_cast<const geo_log::GenericValueLog *>(value_log)) {
    create_inspection_string_for_generic_value(*generic_value_log, ss);
  }
  else if (const geo_log::GeometryValueLog *geo_value_log =
               dynamic_cast<const geo_log::GeometryValueLog *>(value_log)) {
    create_inspection_string_for_geometry(*geo_value_log, ss);
  }

  return ss.str();
}

static void node_socket_draw_nested(const bContext *C,
                                    bNodeTree *ntree,
                                    PointerRNA *node_ptr,
                                    bNodeSocket *sock,
                                    uint pos_id,
                                    uint col_id,
                                    uint shape_id,
                                    uint size_id,
                                    uint outline_col_id,
                                    float size,
                                    bool selected)
{
  float color[4];
  float outline_color[4];

  node_socket_color_get((bContext *)C, ntree, node_ptr, sock, color);
  node_socket_outline_color_get(selected, sock->type, outline_color);

  node_socket_draw(sock,
                   color,
                   outline_color,
                   size,
                   sock->locx,
                   sock->locy,
                   pos_id,
                   col_id,
                   shape_id,
                   size_id,
                   outline_col_id);

  if (ntree->type != NTREE_GEOMETRY) {
    /* Only geometry nodes has socket value tooltips currently. */
    return;
  }

  bNode *node = (bNode *)node_ptr->data;
  uiBlock *block = node->block;

  /* Ideally sockets themselves should be buttons, but they aren't currently. So add an invisible
   * button on top of them for the tooltip. */
  const eUIEmbossType old_emboss = UI_block_emboss_get(block);
  UI_block_emboss_set(block, UI_EMBOSS_NONE);
  uiBut *but = uiDefIconBut(block,
                            UI_BTYPE_BUT,
                            0,
                            ICON_NONE,
                            sock->locx - size / 2,
                            sock->locy - size / 2,
                            size,
                            size,
                            nullptr,
                            0,
                            0,
                            0,
                            0,
                            nullptr);

  SocketTooltipData *data = (SocketTooltipData *)MEM_mallocN(sizeof(SocketTooltipData), __func__);
  data->ntree = ntree;
  data->node = (bNode *)node_ptr->data;
  data->socket = sock;

  UI_but_func_tooltip_set(
      but,
      [](bContext *C, void *argN, const char *UNUSED(tip)) {
        SocketTooltipData *data = (SocketTooltipData *)argN;
        std::optional<std::string> socket_inspection_str = create_socket_inspection_string(
            C, *data->ntree, *data->node, *data->socket);

        std::stringstream output;
        if (data->node->declaration != nullptr) {
          ListBase *list;
          Span<blender::nodes::SocketDeclarationPtr> decl_list;

          if (data->socket->in_out == SOCK_IN) {
            list = &data->node->inputs;
            decl_list = data->node->declaration->inputs();
          }
          else {
            list = &data->node->outputs;
            decl_list = data->node->declaration->outputs();
          }

          const int socket_index = BLI_findindex(list, data->socket);
          const blender::nodes::SocketDeclaration &socket_decl = *decl_list[socket_index];
          blender::StringRef description = socket_decl.description();
          if (!description.is_empty()) {
            output << TIP_(description.data()) << ".\n\n";
          }
        }
        if (socket_inspection_str.has_value()) {
          output << *socket_inspection_str;
        }
        else {
          output << TIP_("The socket value has not been computed yet");
        }
        return BLI_strdup(output.str().c_str());
      },
      data,
      MEM_freeN);
  /* Disable the button so that clicks on it are ignored the the link operator still works. */
  UI_but_flag_enable(but, UI_BUT_DISABLED);
  UI_block_emboss_set(block, old_emboss);
}

/**
 * Draw a single node socket at default size.
 * \note this is only called from external code, internally #node_socket_draw_nested() is used for
 *       optimized drawing of multiple/all sockets of a node.
 */
void ED_node_socket_draw(bNodeSocket *sock, const rcti *rect, const float color[4], float scale)
{
  const float size = 2.25f * NODE_SOCKSIZE * scale;
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
  immUniform1f("outline_scale", 0.7f);
  immUniform2f("ViewportSize", -1.0f, -1.0f);

  /* Single point. */
  immBegin(GPU_PRIM_POINTS, 1);
  node_socket_draw(sock,
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
  float xscale = xrect / ((float)preview->xsize);
  float yscale = yrect / ((float)preview->ysize);
  float scale;

  /* Uniform scale and offset. */
  rctf draw_rect = *prv;
  if (xscale < yscale) {
    float offset = 0.5f * (yrect - ((float)preview->ysize) * xscale);
    draw_rect.ymin += offset;
    draw_rect.ymax -= offset;
    scale = xscale;
  }
  else {
    float offset = 0.5f * (xrect - ((float)preview->xsize) * yscale);
    draw_rect.xmin += offset;
    draw_rect.xmax -= offset;
    scale = yscale;
  }

  node_draw_preview_background(&draw_rect);

  GPU_blend(GPU_BLEND_ALPHA);
  /* Premul graphics. */
  GPU_blend(GPU_BLEND_ALPHA);

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
  immDrawPixelsTex(&state,
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
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColorShadeAlpha(TH_BACK, -15, +100);
  imm_draw_box_wire_2d(pos, draw_rect.xmin, draw_rect.ymin, draw_rect.xmax, draw_rect.ymax);
  immUnbindProgram();
}

/* Common handle function for operator buttons that need to select the node first. */
static void node_toggle_button_cb(struct bContext *C, void *node_argv, void *op_argv)
{
  bNode *node = (bNode *)node_argv;
  const char *opname = (const char *)op_argv;

  /* Select & activate only the button's node. */
  node_select_single(C, node);

  WM_operator_name_call(C, opname, WM_OP_INVOKE_DEFAULT, nullptr);
}

void node_draw_shadow(const SpaceNode *snode, const bNode *node, float radius, float alpha)
{
  const rctf *rct = &node->totr;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  ui_draw_dropshadow(rct, radius, snode->runtime->aspect, alpha, node->flag & SELECT);
}

void node_draw_sockets(const View2D *v2d,
                       const bContext *C,
                       bNodeTree *ntree,
                       bNode *node,
                       bool draw_outputs,
                       bool select_all)
{
  const uint total_input_len = BLI_listbase_count(&node->inputs);
  const uint total_output_len = BLI_listbase_count(&node->outputs);

  if (total_input_len + total_output_len == 0) {
    return;
  }

  PointerRNA node_ptr;
  RNA_pointer_create((ID *)ntree, &RNA_Node, node, &node_ptr);

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
  immUniform1f("outline_scale", 0.7f);
  immUniform2f("ViewportSize", -1.0f, -1.0f);

  /* Set handle size. */
  float scale;
  UI_view2d_scale_get(v2d, &scale, nullptr);
  scale *= 2.25f * NODE_SOCKSIZE;

  if (!select_all) {
    immBeginAtMost(GPU_PRIM_POINTS, total_input_len + total_output_len);
  }

  /* Socket inputs. */
  short selected_input_len = 0;
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (nodeSocketIsHidden(sock)) {
      continue;
    }
    if (select_all || (sock->flag & SELECT)) {
      selected_input_len++;
      continue;
    }
    /* Don't draw multi-input sockets here since they are drawn in a different batch. */
    if (sock->flag & SOCK_MULTI_INPUT) {
      continue;
    }

    node_socket_draw_nested(C,
                            ntree,
                            &node_ptr,
                            sock,
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
    LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
      if (nodeSocketIsHidden(sock)) {
        continue;
      }
      if (select_all || (sock->flag & SELECT)) {
        selected_output_len++;
        continue;
      }

      node_socket_draw_nested(C,
                              ntree,
                              &node_ptr,
                              sock,
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
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        if (nodeSocketIsHidden(sock)) {
          continue;
        }
        if (select_all || (sock->flag & SELECT)) {
          node_socket_draw_nested(C,
                                  ntree,
                                  &node_ptr,
                                  sock,
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
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        if (nodeSocketIsHidden(sock)) {
          continue;
        }
        if (select_all || (sock->flag & SELECT)) {
          node_socket_draw_nested(C,
                                  ntree,
                                  &node_ptr,
                                  sock,
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
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    if (nodeSocketIsHidden(socket)) {
      continue;
    }
    if (!(socket->flag & SOCK_MULTI_INPUT)) {
      continue;
    }

    const bool is_node_hidden = (node->flag & NODE_HIDDEN);
    const float width = NODE_SOCKSIZE;
    float height = is_node_hidden ? width : node_socket_calculate_height(socket) - width;

    float color[4];
    float outline_color[4];
    node_socket_color_get((bContext *)C, ntree, &node_ptr, socket, color);
    node_socket_outline_color_get(selected, socket->type, outline_color);

    node_socket_draw_multi_input(color, outline_color, width, height, socket->locx, socket->locy);
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
    case geo_log::NodeWarningType::Legacy:
      return ICON_ERROR;
  }

  BLI_assert(false);
  return ICON_ERROR;
}

static uint8_t node_error_type_priority(const geo_log::NodeWarningType type)
{
  switch (type) {
    case geo_log::NodeWarningType::Error:
      return 4;
    case geo_log::NodeWarningType::Legacy:
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

static char *node_errors_tooltip_fn(bContext *UNUSED(C), void *argN, const char *UNUSED(tip))
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

static void node_add_error_message_button(
    const bContext *C, bNodeTree &UNUSED(ntree), bNode &node, const rctf &rect, float &icon_offset)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  const geo_log::NodeLog *node_log = geo_log::ModifierLog::find_node_by_node_editor_context(*snode,
                                                                                            node);
  if (node_log == nullptr) {
    return;
  }

  Span<geo_log::NodeWarning> warnings = node_log->warnings();

  if (warnings.is_empty()) {
    return;
  }

  NodeErrorsTooltipData *tooltip_data = (NodeErrorsTooltipData *)MEM_mallocN(
      sizeof(NodeErrorsTooltipData), __func__);
  tooltip_data->warnings = warnings;

  const geo_log::NodeWarningType display_type = node_error_highest_priority(warnings);

  icon_offset -= NODE_HEADER_ICON_SIZE;
  UI_block_emboss_set(node.block, UI_EMBOSS_NONE);
  uiBut *but = uiDefIconBut(node.block,
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
  UI_but_func_tooltip_set(but, node_errors_tooltip_fn, tooltip_data, MEM_freeN);
  UI_block_emboss_set(node.block, UI_EMBOSS);
}

static void node_draw_basis(const bContext *C,
                            const View2D *v2d,
                            const SpaceNode *snode,
                            bNodeTree *ntree,
                            bNode *node,
                            bNodeInstanceKey key)
{
  const float iconbutw = NODE_HEADER_ICON_SIZE;

  /* Skip if out of view. */
  if (BLI_rctf_isect(&node->totr, &v2d->cur, nullptr) == false) {
    UI_block_end(C, node->block);
    node->block = nullptr;
    return;
  }

  /* Shadow. */
  node_draw_shadow(snode, node, BASIS_RAD, 1.0f);

  float color[4];
  int color_id = node_get_colorid(node);
  if (node->flag & NODE_MUTED) {
    /* Muted nodes are semi-transparent and colorless. */
    UI_GetThemeColor3fv(TH_NODE, color);
    color[3] = 0.25f;
  }
  else {
    /* Opaque headers for regular nodes. */
    UI_GetThemeColor3fv(color_id, color);
    color[3] = 1.0f;
  }

  GPU_line_width(1.0f);

  rctf *rct = &node->totr;
  UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);

  {
    const rctf rect = {
        rct->xmin,
        rct->xmax,
        rct->ymax - NODE_DY,
        rct->ymax,
    };
    UI_draw_roundbox_aa(&rect, true, BASIS_RAD, color);
  }

  /* Show/hide icons. */
  float iconofs = rct->xmax - 0.35f * U.widget_unit;

  /* Preview. */
  if (node->typeinfo->flag & NODE_PREVIEW) {
    iconofs -= iconbutw;
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(node->block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_MATERIAL,
                              iconofs,
                              rct->ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              0,
                              0,
                              "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_preview_toggle");
    /* XXX this does not work when node is activated and the operator called right afterwards,
     * since active ID is not updated yet (needs to process the notifier).
     * This can only work as visual indicator! */
    //      if (!(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT)))
    //          UI_but_flag_enable(but, UI_BUT_DISABLED);
    UI_block_emboss_set(node->block, UI_EMBOSS);
  }
  /* Group edit. */
  if (node->type == NODE_GROUP) {
    iconofs -= iconbutw;
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    uiBut *but = uiDefIconBut(node->block,
                              UI_BTYPE_BUT_TOGGLE,
                              0,
                              ICON_NODETREE,
                              iconofs,
                              rct->ymax - NODE_DY,
                              iconbutw,
                              UI_UNIT_Y,
                              nullptr,
                              0,
                              0,
                              0,
                              0,
                              "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_group_edit");
    UI_block_emboss_set(node->block, UI_EMBOSS);
  }
  if (node->type == NODE_CUSTOM && node->typeinfo->ui_icon != ICON_NONE) {
    iconofs -= iconbutw;
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    uiDefIconBut(node->block,
                 UI_BTYPE_BUT,
                 0,
                 node->typeinfo->ui_icon,
                 iconofs,
                 rct->ymax - NODE_DY,
                 iconbutw,
                 UI_UNIT_Y,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 "");
    UI_block_emboss_set(node->block, UI_EMBOSS);
  }

  node_add_error_message_button(C, *ntree, *node, *rct, iconofs);

  /* Title. */
  if (node->flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* Open/close entirely. */
  {
    int but_size = U.widget_unit * 0.8f;
    /* XXX button uses a custom triangle draw below, so make it invisible without icon. */
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    uiBut *but = uiDefBut(node->block,
                          UI_BTYPE_BUT_TOGGLE,
                          0,
                          "",
                          rct->xmin + 0.35f * U.widget_unit,
                          rct->ymax - NODE_DY / 2.2f - but_size / 2,
                          but_size,
                          but_size,
                          nullptr,
                          0,
                          0,
                          0,
                          0,
                          "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(node->block, UI_EMBOSS);

    UI_GetThemeColor4fv(TH_TEXT, color);
    /* Custom draw function for this button. */
    UI_draw_icon_tri(rct->xmin + 0.65f * U.widget_unit, rct->ymax - NODE_DY / 2.2f, 'v', color);
  }

  char showname[128];
  nodeLabel(ntree, node, showname, sizeof(showname));

  uiBut *but = uiDefBut(node->block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        (int)(rct->xmin + NODE_MARGIN_X),
                        (int)(rct->ymax - NODE_DY),
                        (short)(iconofs - rct->xmin - (18.0f * U.dpi_fac)),
                        (short)NODE_DY,
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        "");
  if (node->flag & NODE_MUTED) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* Body. */
  if (nodeTypeUndefined(node)) {
    /* Use warning color to indicate undefined types. */
    UI_GetThemeColor4fv(TH_REDALERT, color);
  }
  else if (node->flag & NODE_MUTED) {
    /* Muted nodes are semi-transparent and colorless. */
    UI_GetThemeColor4fv(TH_NODE, color);
  }
  else if (node->flag & NODE_CUSTOM_COLOR) {
    rgba_float_args_set(color, node->color[0], node->color[1], node->color[2], 1.0f);
  }
  else {
    UI_GetThemeColor4fv(TH_NODE, color);
  }

  if (node->flag & NODE_MUTED) {
    color[3] = 0.5f;
  }

  {
    UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT);
    const rctf rect = {
        rct->xmin,
        rct->xmax,
        rct->ymin,
        rct->ymax - NODE_DY,
    };
    UI_draw_roundbox_aa(&rect, true, BASIS_RAD, color);
  }

  /* Outline active and selected emphasis. */
  if (node->flag & SELECT) {
    UI_GetThemeColorShadeAlpha4fv(
        (node->flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, 0, -40, color);

    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_aa(rct, false, BASIS_RAD, color);
  }

  /* Disable lines. */
  if (node->flag & NODE_MUTED) {
    node_draw_mute_line(v2d, snode, node);
  }

  node_draw_sockets(v2d, C, ntree, node, true, false);

  /* Preview. */
  bNodeInstanceHash *previews = (bNodeInstanceHash *)CTX_data_pointer_get(C, "node_previews").data;
  if (node->flag & NODE_PREVIEW && previews) {
    bNodePreview *preview = (bNodePreview *)BKE_node_instance_hash_lookup(previews, key);
    if (preview && (preview->xsize && preview->ysize)) {
      if (preview->rect && !BLI_rctf_is_empty(&node->prvr)) {
        node_draw_preview(preview, &node->prvr);
      }
    }
  }

  UI_block_end(C, node->block);
  UI_block_draw(C, node->block);
  node->block = nullptr;
}

static void node_draw_hidden(const bContext *C,
                             const View2D *v2d,
                             const SpaceNode *snode,
                             bNodeTree *ntree,
                             bNode *node,
                             bNodeInstanceKey UNUSED(key))
{
  rctf *rct = &node->totr;
  float centy = BLI_rctf_cent_y(rct);
  float hiddenrad = BLI_rctf_size_y(rct) / 2.0f;

  float scale;
  UI_view2d_scale_get(v2d, &scale, nullptr);

  /* Shadow. */
  node_draw_shadow(snode, node, hiddenrad, 1.0f);

  /* Body. */
  float color[4];
  int color_id = node_get_colorid(node);
  if (node->flag & NODE_MUTED) {
    /* Muted nodes are semi-transparent and colorless. */
    UI_GetThemeColor4fv(TH_NODE, color);
    color[3] = 0.25f;
  }
  else {
    UI_GetThemeColor4fv(color_id, color);
  }

  UI_draw_roundbox_aa(rct, true, hiddenrad, color);

  /* Outline active and selected emphasis. */
  if (node->flag & SELECT) {
    UI_GetThemeColorShadeAlpha4fv(
        (node->flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, 0, -40, color);

    UI_draw_roundbox_aa(rct, false, hiddenrad, color);
  }

  /* Custom color inline. */
  if (node->flag & NODE_CUSTOM_COLOR) {
    GPU_blend(GPU_BLEND_ALPHA);
    GPU_line_smooth(true);

    const rctf rect = {
        rct->xmin + 1,
        rct->xmax - 1,
        rct->ymin + 1,
        rct->ymax - 1,
    };
    UI_draw_roundbox_3fv_alpha(&rect, false, hiddenrad, node->color, 1.0f);

    GPU_line_smooth(false);
    GPU_blend(GPU_BLEND_NONE);
  }

  /* Title. */
  if (node->flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* Open / collapse icon. */
  {
    int but_size = U.widget_unit * 0.8f;
    /* XXX button uses a custom triangle draw below, so make it invisible without icon */
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    uiBut *but = uiDefBut(node->block,
                          UI_BTYPE_BUT_TOGGLE,
                          0,
                          "",
                          rct->xmin + 0.35f * U.widget_unit,
                          centy - but_size / 2,
                          but_size,
                          but_size,
                          nullptr,
                          0,
                          0,
                          0,
                          0,
                          "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(node->block, UI_EMBOSS);

    UI_GetThemeColor4fv(TH_TEXT, color);
    /* Custom draw function for this button. */
    UI_draw_icon_tri(rct->xmin + 0.65f * U.widget_unit, centy, 'h', color);
  }

  /* Disable lines. */
  if (node->flag & NODE_MUTED) {
    node_draw_mute_line(v2d, snode, node);
  }

  char showname[128];
  nodeLabel(ntree, node, showname, sizeof(showname));

  uiBut *but = uiDefBut(node->block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        round_fl_to_int(rct->xmin + NODE_MARGIN_X),
                        round_fl_to_int(centy - NODE_DY * 0.5f),
                        (short)(BLI_rctf_size_x(rct) - ((18.0f + 12.0f) * U.dpi_fac)),
                        (short)NODE_DY,
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        "");
  if (node->flag & NODE_MUTED) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* Scale widget thing. */
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformThemeColorShade(color_id, -10);
  float dx = 10.0f;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct->xmax - dx, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx, centy + 4.0f);

  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->runtime->aspect, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->runtime->aspect, centy + 4.0f);
  immEnd();

  immUniformThemeColorShade(color_id, 30);
  dx -= snode->runtime->aspect;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct->xmax - dx, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx, centy + 4.0f);

  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->runtime->aspect, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->runtime->aspect, centy + 4.0f);
  immEnd();

  immUnbindProgram();

  node_draw_sockets(v2d, C, ntree, node, true, false);

  UI_block_end(C, node->block);
  UI_block_draw(C, node->block);
  node->block = nullptr;
}

int node_get_resize_cursor(int directions)
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

void node_set_cursor(wmWindow *win, SpaceNode *snode, float cursor[2])
{
  bNodeTree *ntree = snode->edittree;
  bNode *node;
  bNodeSocket *sock;
  int wmcursor = WM_CURSOR_DEFAULT;

  if (ntree) {
    if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_IN | SOCK_OUT)) {
      /* Pass. */
    }
    else {
      /* Check nodes front to back. */
      for (node = (bNode *)ntree->nodes.last; node; node = node->prev) {
        if (BLI_rctf_isect_pt(&node->totr, cursor[0], cursor[1])) {
          break; /* First hit on node stops. */
        }
      }
      if (node) {
        int dir = node->typeinfo->resize_area_func(node, cursor[0], cursor[1]);
        wmcursor = node_get_resize_cursor(dir);
      }
    }
  }

  WM_cursor_set(win, wmcursor);
}

void node_draw_default(const bContext *C,
                       ARegion *region,
                       SpaceNode *snode,
                       bNodeTree *ntree,
                       bNode *node,
                       bNodeInstanceKey key)
{
  const View2D *v2d = &region->v2d;
  if (node->flag & NODE_HIDDEN) {
    node_draw_hidden(C, v2d, snode, ntree, node, key);
  }
  else {
    node_draw_basis(C, v2d, snode, ntree, node, key);
  }
}

static void node_update(const bContext *C, bNodeTree *ntree, bNode *node)
{
  if (node->typeinfo->draw_nodetype_prepare) {
    node->typeinfo->draw_nodetype_prepare(C, ntree, node);
  }
}

static void count_multi_input_socket_links(bNodeTree *ntree, SpaceNode *snode)
{
  Map<bNodeSocket *, int> counts;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tosock->flag & SOCK_MULTI_INPUT) {
      int &count = counts.lookup_or_add(link->tosock, 0);
      count++;
    }
  }
  /* Count temporary links going into this socket. */
  LISTBASE_FOREACH (bNodeLinkDrag *, nldrag, &snode->runtime->linkdrag) {
    LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
      bNodeLink *link = (bNodeLink *)linkdata->data;
      if (link->tosock && (link->tosock->flag & SOCK_MULTI_INPUT)) {
        int &count = counts.lookup_or_add(link->tosock, 0);
        count++;
      }
    }
  }
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      if (socket->flag & SOCK_MULTI_INPUT) {
        socket->total_inputs = counts.lookup_default(socket, 0);
      }
    }
  }
}

void node_update_nodetree(const bContext *C, bNodeTree *ntree)
{
  /* Make sure socket "used" tags are correct, for displaying value buttons. */
  SpaceNode *snode = CTX_wm_space_node(C);
  ntreeTagUsedSockets(ntree);

  count_multi_input_socket_links(ntree, snode);

  /* Update nodes front to back, so children sizes get updated before parents. */
  LISTBASE_FOREACH_BACKWARD (bNode *, node, &ntree->nodes) {
    node_update(C, ntree, node);
  }
}

static void node_draw(const bContext *C,
                      ARegion *region,
                      SpaceNode *snode,
                      bNodeTree *ntree,
                      bNode *node,
                      bNodeInstanceKey key)
{
  if (node->typeinfo->draw_nodetype) {
    node->typeinfo->draw_nodetype(C, region, snode, ntree, node, key);
  }
}

#define USE_DRAW_TOT_UPDATE

void node_draw_nodetree(const bContext *C,
                        ARegion *region,
                        SpaceNode *snode,
                        bNodeTree *ntree,
                        bNodeInstanceKey parent_key)
{
  if (ntree == nullptr) {
    return; /* Groups. */
  }

#ifdef USE_DRAW_TOT_UPDATE
  if (ntree->nodes.first) {
    BLI_rctf_init_minmax(&region->v2d.tot);
  }
#endif

  /* Draw background nodes, last nodes in front. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
#ifdef USE_DRAW_TOT_UPDATE
    /* Unrelated to background nodes, update the v2d->tot,
     * can be anywhere before we draw the scroll bars. */
    BLI_rctf_union(&region->v2d.tot, &node->totr);
#endif

    if (!(node->flag & NODE_BACKGROUND)) {
      continue;
    }

    bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);
    node_draw(C, region, snode, ntree, node, key);
  }

  /* Node lines. */
  GPU_blend(GPU_BLEND_ALPHA);
  nodelink_batch_start(snode);

  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (!nodeLinkIsHidden(link)) {
      node_draw_link(&region->v2d, snode, link);
    }
  }
  nodelink_batch_end(snode);
  GPU_blend(GPU_BLEND_NONE);

  /* Draw foreground nodes, last nodes in front. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->flag & NODE_BACKGROUND) {
      continue;
    }

    bNodeInstanceKey key = BKE_node_instance_key(parent_key, ntree, node);
    node_draw(C, region, snode, ntree, node, key);
  }
}

/* Draw tree path info in lower left corner. */
static void draw_tree_path(SpaceNode *snode)
{
  char info[256];

  ED_node_tree_path_get_fixedbuf(snode, info, sizeof(info));

  UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
  BLF_draw_default(1.5f * UI_UNIT_X, 1.5f * UI_UNIT_Y, 0.0f, info, sizeof(info));
}

static void snode_setup_v2d(SpaceNode *snode, ARegion *region, const float center[2])
{
  View2D *v2d = &region->v2d;

  /* Shift view to node tree center. */
  UI_view2d_center_set(v2d, center[0], center[1]);
  UI_view2d_view_ortho(v2d);

  /* Aspect + font, set each time. */
  snode->runtime->aspect = BLI_rctf_size_x(&v2d->cur) / (float)region->winx;
  // XXX snode->curfont = uiSetCurFont_ext(snode->aspect);
}

static void draw_nodetree(const bContext *C,
                          ARegion *region,
                          bNodeTree *ntree,
                          bNodeInstanceKey parent_key)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  node_uiblocks_init(C, ntree);

  node_update_nodetree(C, ntree);
  node_draw_nodetree(C, region, snode, ntree, parent_key);
}

/**
 * Make the background slightly brighter to indicate that users are inside a node-group.
 */
static void draw_background_color(const SpaceNode *snode)
{
  const int max_tree_length = 3;
  const float bright_factor = 0.25f;

  /* We ignore the first element of the path since it is the top-most tree and it doesn't need to
   * be brighter. We also set a cap to how many levels we want to set apart, to avoid the
   * background from getting too bright. */
  const int clamped_tree_path_length = BLI_listbase_count_at_most(&snode->treepath,
                                                                  max_tree_length);
  const int depth = max_ii(0, clamped_tree_path_length - 1);

  float color[3];
  UI_GetThemeColor3fv(TH_BACK, color);
  mul_v3_fl(color, 1.0f + bright_factor * depth);
  GPU_clear_color(color[0], color[1], color[2], 1.0);
}

void node_draw_space(const bContext *C, ARegion *region)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  View2D *v2d = &region->v2d;

  /* Setup off-screen buffers. */
  GPUViewport *viewport = WM_draw_region_get_viewport(region);

  GPUFrameBuffer *framebuffer_overlay = GPU_viewport_framebuffer_overlay_get(viewport);
  GPU_framebuffer_bind_no_srgb(framebuffer_overlay);

  UI_view2d_view_ortho(v2d);
  draw_background_color(snode);
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_scissor_test(true);

  /* XXX `snode->runtime->cursor` set in coordinate-space for placing new nodes,
   * used for drawing noodles too. */
  UI_view2d_region_to_view(&region->v2d,
                           win->eventstate->xy[0] - region->winrct.xmin,
                           win->eventstate->xy[1] - region->winrct.ymin,
                           &snode->runtime->cursor[0],
                           &snode->runtime->cursor[1]);
  snode->runtime->cursor[0] /= UI_DPI_FAC;
  snode->runtime->cursor[1] /= UI_DPI_FAC;

  int grid_levels = UI_GetThemeValueType(TH_NODE_GRID_LEVELS, SPACE_NODE);

  ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

  /* Only set once. */
  GPU_blend(GPU_BLEND_ALPHA);

  /* Nodes. */
  snode_set_context(C);

  /* Draw parent node trees. */
  if (snode->treepath.last) {
    bNodeTreePath *path = (bNodeTreePath *)snode->treepath.last;

    /* Update tree path name (drawn in the bottom left). */
    ID *name_id = (path->nodetree && path->nodetree != snode->nodetree) ? &path->nodetree->id :
                                                                          snode->id;

    if (name_id && UNLIKELY(!STREQ(path->display_name, name_id->name + 2))) {
      BLI_strncpy(path->display_name, name_id->name + 2, sizeof(path->display_name));
    }

    /* Current View2D center, will be set temporarily for parent node trees. */
    float center[2];
    UI_view2d_center_get(v2d, &center[0], &center[1]);

    /* Store new view center in path and current edit tree. */
    copy_v2_v2(path->view_center, center);
    if (snode->edittree) {
      copy_v2_v2(snode->edittree->view_center, center);
    }

    /* Top-level edit tree. */
    bNodeTree *ntree = path->nodetree;
    if (ntree) {
      snode_setup_v2d(snode, region, center);

      /* Grid. */
      UI_view2d_multi_grid_draw(v2d, TH_GRID, ED_node_grid_size(), NODE_GRID_STEPS, grid_levels);

      /* Backdrop. */
      draw_nodespace_back_pix(C, region, snode, path->parent_key);

      {
        float original_proj[4][4];
        GPU_matrix_projection_get(original_proj);

        GPU_matrix_push();
        GPU_matrix_identity_set();

        wmOrtho2_pixelspace(region->winx, region->winy);

        WM_gizmomap_draw(region->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);

        GPU_matrix_pop();
        GPU_matrix_projection_set(original_proj);
      }

      draw_nodetree(C, region, ntree, path->parent_key);
    }

    /* Temporary links. */
    GPU_blend(GPU_BLEND_ALPHA);
    GPU_line_smooth(true);
    LISTBASE_FOREACH (bNodeLinkDrag *, nldrag, &snode->runtime->linkdrag) {
      LISTBASE_FOREACH (LinkData *, linkdata, &nldrag->links) {
        node_draw_link(v2d, snode, (bNodeLink *)linkdata->data);
      }
    }
    GPU_line_smooth(false);
    GPU_blend(GPU_BLEND_NONE);

    if (snode->overlay.flag & SN_OVERLAY_SHOW_OVERLAYS && snode->flag & SNODE_SHOW_GPENCIL) {
      /* Draw grease-pencil annotations. */
      ED_annotation_draw_view2d(C, true);
    }
  }
  else {
    /* Default grid. */
    UI_view2d_multi_grid_draw(v2d, TH_GRID, ED_node_grid_size(), NODE_GRID_STEPS, grid_levels);

    /* Backdrop. */
    draw_nodespace_back_pix(C, region, snode, NODE_INSTANCE_KEY_NONE);
  }

  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);

  /* Reset view matrix. */
  UI_view2d_view_restore(C);

  if (snode->treepath.last) {
    if (snode->overlay.flag & SN_OVERLAY_SHOW_OVERLAYS && snode->flag & SNODE_SHOW_GPENCIL) {
      /* Draw grease-pencil (screen strokes, and also paint-buffer). */
      ED_annotation_draw_view2d(C, false);
    }
  }

  /* Tree path info. */
  draw_tree_path(snode);

  /* Scrollers. */
  UI_view2d_scrollers_draw(v2d, nullptr);
}
