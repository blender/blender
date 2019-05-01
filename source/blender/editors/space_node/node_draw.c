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

#include "DNA_light_types.h"
#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_linestyle_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DEG_depsgraph.h"

#include "BLF_api.h"

#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_framebuffer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_node.h"
#include "ED_gpencil.h"
#include "ED_space_api.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "node_intern.h" /* own include */

#ifdef WITH_COMPOSITOR
#  include "COM_compositor.h"
#endif

/* XXX interface.h */
extern void ui_draw_dropshadow(
    const rctf *rct, float radius, float aspect, float alpha, int select);

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
    short idtype = GS(id->name);

    switch (idtype) {
      case ID_NT:
        return (bNodeTree *)id;
      case ID_MA:
        return ((Material *)id)->nodetree;
      case ID_LA:
        return ((Light *)id)->nodetree;
      case ID_WO:
        return ((World *)id)->nodetree;
      case ID_SCE:
        return ((Scene *)id)->nodetree;
      case ID_TE:
        return ((Tex *)id)->nodetree;
      case ID_LS:
        return ((FreestyleLineStyle *)id)->nodetree;
    }
  }

  return NULL;
}

void ED_node_tag_update_id(ID *id)
{
  bNodeTree *ntree = node_tree_from_ID(id);
  if (id == NULL || ntree == NULL) {
    return;
  }

  /* TODO(sergey): With the new dependency graph it
   * should be just enough to only tag ntree itself,
   * all the users of this tree will have update
   * flushed from the tree,
   */
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
  else if (id == &ntree->id) {
    /* node groups */
    DEG_id_tag_update(id, 0);
  }
}

void ED_node_tag_update_nodetree(Main *bmain, bNodeTree *ntree, bNode *node)
{
  if (!ntree) {
    return;
  }

  bool do_tag_update = true;
  if (node != NULL) {
    if (!node_connected_to_output(bmain, ntree, node)) {
      do_tag_update = false;
    }
  }

  /* look through all datablocks, to support groups */
  if (do_tag_update) {
    FOREACH_NODETREE_BEGIN (bmain, tntree, id) {
      /* check if nodetree uses the group */
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
  bNode *parent;
  /* These tell if either the node or any of the parent nodes is selected.
   * A selected parent means an unselected node is also in foreground!
   */
  bool a_select = (a->flag & NODE_SELECT) != 0, b_select = (b->flag & NODE_SELECT) != 0;
  bool a_active = (a->flag & NODE_ACTIVE) != 0, b_active = (b->flag & NODE_ACTIVE) != 0;

  /* if one is an ancestor of the other */
  /* XXX there might be a better sorting algorithm for stable topological sort,
   * this is O(n^2) worst case */
  for (parent = a->parent; parent; parent = parent->parent) {
    /* if b is an ancestor, it is always behind a */
    if (parent == b) {
      return 1;
    }
    /* any selected ancestor moves the node forward */
    if (parent->flag & NODE_ACTIVE) {
      a_active = 1;
    }
    if (parent->flag & NODE_SELECT) {
      a_select = 1;
    }
  }
  for (parent = b->parent; parent; parent = parent->parent) {
    /* if a is an ancestor, it is always behind b */
    if (parent == a) {
      return 0;
    }
    /* any selected ancestor moves the node forward */
    if (parent->flag & NODE_ACTIVE) {
      b_active = 1;
    }
    if (parent->flag & NODE_SELECT) {
      b_select = 1;
    }
  }

  /* if one of the nodes is in the background and the other not */
  if ((a->flag & NODE_BACKGROUND) && !(b->flag & NODE_BACKGROUND)) {
    return 0;
  }
  else if (!(a->flag & NODE_BACKGROUND) && (b->flag & NODE_BACKGROUND)) {
    return 1;
  }

  /* if one has a higher selection state (active > selected > nothing) */
  if (!b_active && a_active) {
    return 1;
  }
  else if (!b_select && (a_active || a_select)) {
    return 1;
  }

  return 0;
}

/* Sorts nodes by selection: unselected nodes first, then selected,
 * then the active node at the very end. Relative order is kept intact!
 */
void ED_node_sort(bNodeTree *ntree)
{
  /* merge sort is the algorithm of choice here */
  bNode *first_a, *first_b, *node_a, *node_b, *tmp;
  int totnodes = BLI_listbase_count(&ntree->nodes);
  int k, a, b;

  k = 1;
  while (k < totnodes) {
    first_a = first_b = ntree->nodes.first;

    do {
      /* setup first_b pointer */
      for (b = 0; b < k && first_b; ++b) {
        first_b = first_b->next;
      }
      /* all batches merged? */
      if (first_b == NULL) {
        break;
      }

      /* merge batches */
      node_a = first_a;
      node_b = first_b;
      a = b = 0;
      while (a < k && b < k && node_b) {
        if (compare_nodes(node_a, node_b) == 0) {
          node_a = node_a->next;
          a++;
        }
        else {
          tmp = node_b;
          node_b = node_b->next;
          b++;
          BLI_remlink(&ntree->nodes, tmp);
          BLI_insertlinkbefore(&ntree->nodes, node_a, tmp);
        }
      }

      /* setup first pointers for next batch */
      first_b = node_b;
      for (; b < k; ++b) {
        /* all nodes sorted? */
        if (first_b == NULL) {
          break;
        }
        first_b = first_b->next;
      }
      first_a = first_b;
    } while (first_b);

    k = k << 1;
  }
}

static void do_node_internal_buttons(bContext *C, void *UNUSED(node_v), int event)
{
  if (event == B_NODE_EXEC) {
    SpaceNode *snode = CTX_wm_space_node(C);
    if (snode && snode->id) {
      ED_node_tag_update_id(snode->id);
    }
  }
}

static void node_uiblocks_init(const bContext *C, bNodeTree *ntree)
{
  bNode *node;
  char uiblockstr[32];

  /* add node uiBlocks in drawing order - prevents events going to overlapping nodes */

  for (node = ntree->nodes.first; node; node = node->next) {
    /* ui block */
    BLI_snprintf(uiblockstr, sizeof(uiblockstr), "node buttons %p", (void *)node);
    node->block = UI_block_begin(C, CTX_wm_region(C), uiblockstr, UI_EMBOSS);
    UI_block_func_handle_set(node->block, do_node_internal_buttons, node);

    /* this cancels events for background nodes */
    UI_block_flag_enable(node->block, UI_BLOCK_CLIP_EVENTS);
  }
}

void node_to_view(struct bNode *node, float x, float y, float *rx, float *ry)
{
  nodeToView(node, x, y, rx, ry);
  *rx *= UI_DPI_FAC;
  *ry *= UI_DPI_FAC;
}

void node_to_updated_rect(struct bNode *node, rctf *r_rect)
{
  node_to_view(node, node->offsetx, node->offsety, &r_rect->xmin, &r_rect->ymax);
  node_to_view(node,
               node->offsetx + node->width,
               node->offsety - node->height,
               &r_rect->xmax,
               &r_rect->ymin);
}

void node_from_view(struct bNode *node, float x, float y, float *rx, float *ry)
{
  x /= UI_DPI_FAC;
  y /= UI_DPI_FAC;
  nodeFromView(node, x, y, rx, ry);
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_basis(const bContext *C, bNodeTree *ntree, bNode *node)
{
  uiLayout *layout, *row;
  PointerRNA nodeptr, sockptr;
  bNodeSocket *nsock;
  float locx, locy;
  float dy;
  int buty;

  RNA_pointer_create(&ntree->id, &RNA_Node, node, &nodeptr);

  /* get "global" coords */
  node_to_view(node, 0.0f, 0.0f, &locx, &locy);
  dy = locy;

  /* header */
  dy -= NODE_DY;

  /* little bit space in top */
  if (node->outputs.first) {
    dy -= NODE_DYS / 2;
  }

  /* output sockets */
  bool add_output_space = false;

  for (nsock = node->outputs.first; nsock; nsock = nsock->next) {
    if (nodeSocketIsHidden(nsock)) {
      continue;
    }

    RNA_pointer_create(&ntree->id, &RNA_NodeSocket, nsock, &sockptr);

    layout = UI_block_layout(node->block,
                             UI_LAYOUT_VERTICAL,
                             UI_LAYOUT_PANEL,
                             locx + NODE_DYS,
                             dy,
                             NODE_WIDTH(node) - NODE_DY,
                             NODE_DY,
                             0,
                             UI_style_get());

    if (node->flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    /* context pointers for current node and socket */
    uiLayoutSetContextPointer(layout, "node", &nodeptr);
    uiLayoutSetContextPointer(layout, "socket", &sockptr);

    /* align output buttons to the right */
    row = uiLayoutRow(layout, 1);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);

    nsock->typeinfo->draw((bContext *)C, row, &sockptr, &nodeptr, IFACE_(nsock->name));

    UI_block_align_end(node->block);
    UI_block_layout_resolve(node->block, NULL, &buty);

    /* ensure minimum socket height in case layout is empty */
    buty = min_ii(buty, dy - NODE_DY);

    nsock->locx = locx + NODE_WIDTH(node);
    /* place the socket circle in the middle of the layout */
    nsock->locy = 0.5f * (dy + buty);

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
      /* width correction of image */
      /* XXX huh? (ton) */
      float dx = (NODE_WIDTH(node) - NODE_DYS) - (NODE_WIDTH(node) - NODE_DYS) / aspect;

      node->prvr.ymin = dy - (NODE_WIDTH(node) - NODE_DY);

      node->prvr.xmin += 0.5f * dx;
      node->prvr.xmax -= 0.5f * dx;
    }

    dy = node->prvr.ymin - NODE_DYS / 2;

    /* make sure that maximums are bigger or equal to minimums */
    if (node->prvr.xmax < node->prvr.xmin) {
      SWAP(float, node->prvr.xmax, node->prvr.xmin);
    }
    if (node->prvr.ymax < node->prvr.ymin) {
      SWAP(float, node->prvr.ymax, node->prvr.ymin);
    }
  }

  /* buttons rect? */
  if (node->typeinfo->draw_buttons && (node->flag & NODE_OPTIONS)) {
    dy -= NODE_DYS / 2;

    /* set this for uifunc() that don't use layout engine yet */
    node->butr.xmin = 0;
    node->butr.xmax = NODE_WIDTH(node) - 2 * NODE_DYS;
    node->butr.ymin = 0;
    node->butr.ymax = 0;

    layout = UI_block_layout(node->block,
                             UI_LAYOUT_VERTICAL,
                             UI_LAYOUT_PANEL,
                             locx + NODE_DYS,
                             dy,
                             node->butr.xmax,
                             0,
                             0,
                             UI_style_get());

    if (node->flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    uiLayoutSetContextPointer(layout, "node", &nodeptr);

    node->typeinfo->draw_buttons(layout, (bContext *)C, &nodeptr);

    UI_block_align_end(node->block);
    UI_block_layout_resolve(node->block, NULL, &buty);

    dy = buty - NODE_DYS / 2;
  }

  /* input sockets */
  for (nsock = node->inputs.first; nsock; nsock = nsock->next) {
    if (nodeSocketIsHidden(nsock)) {
      continue;
    }

    RNA_pointer_create(&ntree->id, &RNA_NodeSocket, nsock, &sockptr);

    layout = UI_block_layout(node->block,
                             UI_LAYOUT_VERTICAL,
                             UI_LAYOUT_PANEL,
                             locx + NODE_DYS,
                             dy,
                             NODE_WIDTH(node) - NODE_DY,
                             NODE_DY,
                             0,
                             UI_style_get());

    if (node->flag & NODE_MUTED) {
      uiLayoutSetActive(layout, false);
    }

    /* context pointers for current node and socket */
    uiLayoutSetContextPointer(layout, "node", &nodeptr);
    uiLayoutSetContextPointer(layout, "socket", &sockptr);

    row = uiLayoutRow(layout, 1);

    nsock->typeinfo->draw((bContext *)C, row, &sockptr, &nodeptr, IFACE_(nsock->name));

    UI_block_align_end(node->block);
    UI_block_layout_resolve(node->block, NULL, &buty);

    /* ensure minimum socket height in case layout is empty */
    buty = min_ii(buty, dy - NODE_DY);

    nsock->locx = locx;
    /* place the socket circle in the middle of the layout */
    nsock->locy = 0.5f * (dy + buty);

    dy = buty;
    if (nsock->next) {
      dy -= NODE_SOCKDY;
    }
  }

  /* little bit space in end */
  if (node->inputs.first || (node->flag & (NODE_OPTIONS | NODE_PREVIEW)) == 0) {
    dy -= NODE_DYS / 2;
  }

  node->totr.xmin = locx;
  node->totr.xmax = locx + NODE_WIDTH(node);
  node->totr.ymax = locy;
  node->totr.ymin = min_ff(dy, locy - 2 * NODE_DY);

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side.
   */
  UI_block_bounds_set_explicit(node->block,
                               node->totr.xmin - NODE_SOCKSIZE,
                               node->totr.ymin,
                               node->totr.xmax + NODE_SOCKSIZE,
                               node->totr.ymax);
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_hidden(bNode *node)
{
  bNodeSocket *nsock;
  float locx, locy;
  float rad, drad, hiddenrad = HIDDEN_RAD;
  int totin = 0, totout = 0, tot;

  /* get "global" coords */
  node_to_view(node, 0.0f, 0.0f, &locx, &locy);

  /* calculate minimal radius */
  for (nsock = node->inputs.first; nsock; nsock = nsock->next) {
    if (!nodeSocketIsHidden(nsock)) {
      totin++;
    }
  }
  for (nsock = node->outputs.first; nsock; nsock = nsock->next) {
    if (!nodeSocketIsHidden(nsock)) {
      totout++;
    }
  }

  tot = MAX2(totin, totout);
  if (tot > 4) {
    hiddenrad += 5.0f * (float)(tot - 4);
  }

  node->totr.xmin = locx;
  node->totr.xmax = locx + max_ff(NODE_WIDTH(node), 2 * hiddenrad);
  node->totr.ymax = locy + (hiddenrad - 0.5f * NODE_DY);
  node->totr.ymin = node->totr.ymax - 2 * hiddenrad;

  /* output sockets */
  rad = drad = (float)M_PI / (1.0f + (float)totout);

  for (nsock = node->outputs.first; nsock; nsock = nsock->next) {
    if (!nodeSocketIsHidden(nsock)) {
      nsock->locx = node->totr.xmax - hiddenrad + sinf(rad) * hiddenrad;
      nsock->locy = node->totr.ymin + hiddenrad + cosf(rad) * hiddenrad;
      rad += drad;
    }
  }

  /* input sockets */
  rad = drad = -(float)M_PI / (1.0f + (float)totin);

  for (nsock = node->inputs.first; nsock; nsock = nsock->next) {
    if (!nodeSocketIsHidden(nsock)) {
      nsock->locx = node->totr.xmin + hiddenrad + sinf(rad) * hiddenrad;
      nsock->locy = node->totr.ymin + hiddenrad + cosf(rad) * hiddenrad;
      rad += drad;
    }
  }

  /* Set the block bounds to clip mouse events from underlying nodes.
   * Add a margin for sockets on each side.
   */
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
    case NODE_CLASS_CONVERTOR:
      return TH_NODE_CONVERTOR;
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
    default:
      return TH_NODE;
  }
}

/* note: in cmp_util.c is similar code, for node_compo_pass_on()
 *       the same goes for shader and texture nodes. */
/* note: in node_edit.c is similar code, for untangle node */
static void node_draw_mute_line(View2D *v2d, SpaceNode *snode, bNode *node)
{
  bNodeLink *link;

  GPU_blend(true);

  for (link = node->internal_links.first; link; link = link->next) {
    node_draw_link_bezier(v2d, snode, link, TH_REDALERT, TH_REDALERT, -1);
  }

  GPU_blend(false);
}

static void node_socket_circle_draw(const bContext *C,
                                    bNodeTree *ntree,
                                    PointerRNA node_ptr,
                                    bNodeSocket *sock,
                                    unsigned pos,
                                    unsigned col)
{
  PointerRNA ptr;
  float color[4];

  RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
  sock->typeinfo->draw_color((bContext *)C, &ptr, &node_ptr, color);

  bNode *node = node_ptr.data;
  if (node->flag & NODE_MUTED) {
    color[3] *= 0.25f;
  }

  immAttr4fv(col, color);
  immVertex2f(pos, sock->locx, sock->locy);
}

/* **************  Socket callbacks *********** */

static void node_draw_preview_background(float tile, rctf *rect)
{
  float x, y;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* draw checkerboard backdrop to show alpha */
  immUniformColor3ub(120, 120, 120);
  immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
  immUniformColor3ub(160, 160, 160);

  for (y = rect->ymin; y < rect->ymax; y += tile * 2) {
    for (x = rect->xmin; x < rect->xmax; x += tile * 2) {
      float tilex = tile, tiley = tile;

      if (x + tile > rect->xmax) {
        tilex = rect->xmax - x;
      }
      if (y + tile > rect->ymax) {
        tiley = rect->ymax - y;
      }

      immRectf(pos, x, y, x + tilex, y + tiley);
    }
  }
  for (y = rect->ymin + tile; y < rect->ymax; y += tile * 2) {
    for (x = rect->xmin + tile; x < rect->xmax; x += tile * 2) {
      float tilex = tile, tiley = tile;

      if (x + tile > rect->xmax) {
        tilex = rect->xmax - x;
      }
      if (y + tile > rect->ymax) {
        tiley = rect->ymax - y;
      }

      immRectf(pos, x, y, x + tilex, y + tiley);
    }
  }
  immUnbindProgram();
}

/* not a callback */
static void node_draw_preview(bNodePreview *preview, rctf *prv)
{
  float xrect = BLI_rctf_size_x(prv);
  float yrect = BLI_rctf_size_y(prv);
  float xscale = xrect / ((float)preview->xsize);
  float yscale = yrect / ((float)preview->ysize);
  float scale;
  rctf draw_rect;

  /* uniform scale and offset */
  draw_rect = *prv;
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

  node_draw_preview_background(BLI_rctf_size_x(prv) / 10.0f, &draw_rect);

  GPU_blend(true);
  /* premul graphics */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
  immDrawPixelsTex(&state,
                   draw_rect.xmin,
                   draw_rect.ymin,
                   preview->xsize,
                   preview->ysize,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   GL_LINEAR,
                   preview->rect,
                   scale,
                   scale,
                   NULL);

  GPU_blend(false);

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColorShadeAlpha(TH_BACK, -15, +100);
  imm_draw_box_wire_2d(pos, draw_rect.xmin, draw_rect.ymin, draw_rect.xmax, draw_rect.ymax);
  immUnbindProgram();
}

/* common handle function for operator buttons that need to select the node first */
static void node_toggle_button_cb(struct bContext *C, void *node_argv, void *op_argv)
{
  bNode *node = (bNode *)node_argv;
  const char *opname = (const char *)op_argv;

  /* select & activate only the button's node */
  node_select_single(C, node);

  WM_operator_name_call(C, opname, WM_OP_INVOKE_DEFAULT, NULL);
}

void node_draw_shadow(SpaceNode *snode, bNode *node, float radius, float alpha)
{
  rctf *rct = &node->totr;

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  if (node->parent == NULL) {
    ui_draw_dropshadow(rct, radius, snode->aspect, alpha, node->flag & SELECT);
  }
  else {
    const float margin = 3.0f;

    float color[4] = {0.0f, 0.0f, 0.0f, 0.33f};
    UI_draw_roundbox_aa(true,
                        rct->xmin - margin,
                        rct->ymin - margin,
                        rct->xmax + margin,
                        rct->ymax + margin,
                        radius + margin,
                        color);
  }
}

void node_draw_sockets(View2D *v2d,
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

  float scale;
  UI_view2d_scale_get(v2d, &scale, NULL);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  GPU_blend(true);
  GPU_enable_program_point_size();

  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_VARYING_COLOR_OUTLINE_AA);

  /* set handle size */
  immUniform1f("size", 2.0f * NODE_SOCKSIZE * scale); /* 2 * size to have diameter */

  if (!select_all) {
    /* outline for unselected sockets */
    immUniform1f("outlineWidth", 1.0f);
    immUniform4f("outlineColor", 0.0f, 0.0f, 0.0f, 0.6f);

    immBeginAtMost(GPU_PRIM_POINTS, total_input_len + total_output_len);
  }

  /* socket inputs */
  short selected_input_len = 0;
  bNodeSocket *sock;
  for (sock = node->inputs.first; sock; sock = sock->next) {
    if (nodeSocketIsHidden(sock)) {
      continue;
    }
    if (select_all || (sock->flag & SELECT)) {
      ++selected_input_len;
      continue;
    }

    node_socket_circle_draw(C, ntree, node_ptr, sock, pos, col);
  }

  /* socket outputs */
  short selected_output_len = 0;
  if (draw_outputs) {
    for (sock = node->outputs.first; sock; sock = sock->next) {
      if (nodeSocketIsHidden(sock)) {
        continue;
      }
      if (select_all || (sock->flag & SELECT)) {
        ++selected_output_len;
        continue;
      }

      node_socket_circle_draw(C, ntree, node_ptr, sock, pos, col);
    }
  }

  if (!select_all) {
    immEnd();
  }

  /* go back and draw selected sockets */
  if (selected_input_len + selected_output_len > 0) {
    /* outline for selected sockets */
    float c[3];
    UI_GetThemeColor3fv(TH_TEXT_HI, c);
    immUniform4f("outlineColor", c[0], c[1], c[2], 1.0f);
    immUniform1f("outlineWidth", 1.5f);

    immBegin(GPU_PRIM_POINTS, selected_input_len + selected_output_len);

    if (selected_input_len) {
      /* socket inputs */
      for (sock = node->inputs.first; sock; sock = sock->next) {
        if (nodeSocketIsHidden(sock)) {
          continue;
        }
        if (select_all || (sock->flag & SELECT)) {
          node_socket_circle_draw(C, ntree, node_ptr, sock, pos, col);
          if (--selected_input_len == 0) {
            break; /* stop as soon as last one is drawn */
          }
        }
      }
    }

    if (selected_output_len) {
      /* socket outputs */
      for (sock = node->outputs.first; sock; sock = sock->next) {
        if (nodeSocketIsHidden(sock)) {
          continue;
        }
        if (select_all || (sock->flag & SELECT)) {
          node_socket_circle_draw(C, ntree, node_ptr, sock, pos, col);
          if (--selected_output_len == 0) {
            break; /* stop as soon as last one is drawn */
          }
        }
      }
    }

    immEnd();
  }

  immUnbindProgram();

  GPU_disable_program_point_size();
  GPU_blend(false);
}

static void node_draw_basis(const bContext *C,
                            ARegion *ar,
                            SpaceNode *snode,
                            bNodeTree *ntree,
                            bNode *node,
                            bNodeInstanceKey key)
{
  bNodeInstanceHash *previews = CTX_data_pointer_get(C, "node_previews").data;
  rctf *rct = &node->totr;
  float iconofs;
  /* float socket_size = NODE_SOCKSIZE*U.dpi/72; */ /* UNUSED */
  float iconbutw = 0.8f * UI_UNIT_X;
  int color_id = node_get_colorid(node);
  float color[4];
  char showname[128]; /* 128 used below */
  View2D *v2d = &ar->v2d;

  /* skip if out of view */
  if (BLI_rctf_isect(&node->totr, &v2d->cur, NULL) == false) {
    UI_block_end(C, node->block);
    node->block = NULL;
    return;
  }

  /* shadow */
  node_draw_shadow(snode, node, BASIS_RAD, 1.0f);

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

  UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
  UI_draw_roundbox_aa(
      true, rct->xmin, rct->ymax - NODE_DY, rct->xmax, rct->ymax, BASIS_RAD, color);

  /* show/hide icons */
  iconofs = rct->xmax - 0.35f * U.widget_unit;

  /* preview */
  if (node->typeinfo->flag & NODE_PREVIEW) {
    uiBut *but;
    iconofs -= iconbutw;
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    but = uiDefIconBut(node->block,
                       UI_BTYPE_BUT_TOGGLE,
                       B_REDR,
                       ICON_MATERIAL,
                       iconofs,
                       rct->ymax - NODE_DY,
                       iconbutw,
                       UI_UNIT_Y,
                       NULL,
                       0,
                       0,
                       0,
                       0,
                       "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_preview_toggle");
    /* XXX this does not work when node is activated and the operator called right afterwards,
     * since active ID is not updated yet (needs to process the notifier).
     * This can only work as visual indicator!
     */
    //      if (!(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT)))
    //          UI_but_flag_enable(but, UI_BUT_DISABLED);
    UI_block_emboss_set(node->block, UI_EMBOSS);
  }
  /* group edit */
  if (node->type == NODE_GROUP) {
    uiBut *but;
    iconofs -= iconbutw;
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    but = uiDefIconBut(node->block,
                       UI_BTYPE_BUT_TOGGLE,
                       B_REDR,
                       ICON_NODETREE,
                       iconofs,
                       rct->ymax - NODE_DY,
                       iconbutw,
                       UI_UNIT_Y,
                       NULL,
                       0,
                       0,
                       0,
                       0,
                       "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_group_edit");
    UI_block_emboss_set(node->block, UI_EMBOSS);
  }

  /* title */
  if (node->flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* open/close entirely? */
  {
    uiBut *but;
    int but_size = U.widget_unit * 0.8f;
    /* XXX button uses a custom triangle draw below, so make it invisible without icon */
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    but = uiDefBut(node->block,
                   UI_BTYPE_BUT_TOGGLE,
                   B_REDR,
                   "",
                   rct->xmin + 0.35f * U.widget_unit,
                   rct->ymax - NODE_DY / 2.2f - but_size / 2,
                   but_size,
                   but_size,
                   NULL,
                   0,
                   0,
                   0,
                   0,
                   "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(node->block, UI_EMBOSS);

    UI_GetThemeColor4fv(TH_TEXT, color);
    /* custom draw function for this button */
    UI_draw_icon_tri(rct->xmin + 0.65f * U.widget_unit, rct->ymax - NODE_DY / 2.2f, 'v', color);
  }

  nodeLabel(ntree, node, showname, sizeof(showname));

  uiBut *but = uiDefBut(node->block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        (int)(rct->xmin + (NODE_MARGIN_X)),
                        (int)(rct->ymax - NODE_DY),
                        (short)(iconofs - rct->xmin - 18.0f),
                        (short)NODE_DY,
                        NULL,
                        0,
                        0,
                        0,
                        0,
                        "");
  if (node->flag & NODE_MUTED) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* body */
  if (!nodeIsRegistered(node)) {
    /* use warning color to indicate undefined types */
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

  UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT);
  UI_draw_roundbox_aa(
      true, rct->xmin, rct->ymin, rct->xmax, rct->ymax - NODE_DY, BASIS_RAD, color);

  /* outline active and selected emphasis */
  if (node->flag & SELECT) {
    UI_GetThemeColorShadeAlpha4fv(
        (node->flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, 0, -40, color);

    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_aa(false, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD, color);
  }

  /* disable lines */
  if (node->flag & NODE_MUTED) {
    node_draw_mute_line(v2d, snode, node);
  }

  node_draw_sockets(v2d, C, ntree, node, true, false);

  /* preview */
  if (node->flag & NODE_PREVIEW && previews) {
    bNodePreview *preview = BKE_node_instance_hash_lookup(previews, key);
    if (preview && (preview->xsize && preview->ysize)) {
      if (preview->rect && !BLI_rctf_is_empty(&node->prvr)) {
        node_draw_preview(preview, &node->prvr);
      }
    }
  }

  UI_ThemeClearColor(color_id);

  UI_block_end(C, node->block);
  UI_block_draw(C, node->block);
  node->block = NULL;
}

static void node_draw_hidden(const bContext *C,
                             ARegion *ar,
                             SpaceNode *snode,
                             bNodeTree *ntree,
                             bNode *node,
                             bNodeInstanceKey UNUSED(key))
{
  rctf *rct = &node->totr;
  float dx, centy = BLI_rctf_cent_y(rct);
  float hiddenrad = BLI_rctf_size_y(rct) / 2.0f;
  int color_id = node_get_colorid(node);
  float color[4];
  char showname[128]; /* 128 is used below */
  View2D *v2d = &ar->v2d;
  float scale;

  UI_view2d_scale_get(v2d, &scale, NULL);

  /* shadow */
  node_draw_shadow(snode, node, hiddenrad, 1.0f);

  /* body */
  if (node->flag & NODE_MUTED) {
    /* Muted nodes are semi-transparent and colorless. */
    UI_GetThemeColor4fv(TH_NODE, color);
    color[3] = 0.25f;
  }
  else {
    UI_GetThemeColor4fv(color_id, color);
  }

  UI_draw_roundbox_aa(true, rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad, color);

  /* outline active and selected emphasis */
  if (node->flag & SELECT) {
    UI_GetThemeColorShadeAlpha4fv(
        (node->flag & NODE_ACTIVE) ? TH_ACTIVE : TH_SELECT, 0, -40, color);

    UI_draw_roundbox_aa(false, rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad, color);
  }

  /* custom color inline */
  if (node->flag & NODE_CUSTOM_COLOR) {
    GPU_blend(true);
    GPU_line_smooth(true);

    UI_draw_roundbox_3fvAlpha(false,
                              rct->xmin + 1,
                              rct->ymin + 1,
                              rct->xmax - 1,
                              rct->ymax - 1,
                              hiddenrad,
                              node->color,
                              1.0f);

    GPU_line_smooth(false);
    GPU_blend(false);
  }

  /* title */
  if (node->flag & SELECT) {
    UI_GetThemeColor4fv(TH_SELECT, color);
  }
  else {
    UI_GetThemeColorBlendShade4fv(TH_SELECT, color_id, 0.4f, 10, color);
  }

  /* open entirely icon */
  {
    uiBut *but;
    int but_size = U.widget_unit * 0.8f;
    /* XXX button uses a custom triangle draw below, so make it invisible without icon */
    UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
    but = uiDefBut(node->block,
                   UI_BTYPE_BUT_TOGGLE,
                   B_REDR,
                   "",
                   rct->xmin + 0.35f * U.widget_unit,
                   centy - but_size / 2,
                   but_size,
                   but_size,
                   NULL,
                   0,
                   0,
                   0,
                   0,
                   "");
    UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_hide_toggle");
    UI_block_emboss_set(node->block, UI_EMBOSS);

    UI_GetThemeColor4fv(TH_TEXT, color);
    /* custom draw function for this button */
    UI_draw_icon_tri(rct->xmin + 0.65f * U.widget_unit, centy, 'h', color);
  }

  /* disable lines */
  if (node->flag & NODE_MUTED) {
    node_draw_mute_line(&ar->v2d, snode, node);
  }

  nodeLabel(ntree, node, showname, sizeof(showname));

  /* XXX - don't print into self! */
  // if (node->flag & NODE_MUTED)
  //  BLI_snprintf(showname, sizeof(showname), "[%s]", showname);

  uiBut *but = uiDefBut(node->block,
                        UI_BTYPE_LABEL,
                        0,
                        showname,
                        round_fl_to_int(rct->xmin + NODE_MARGIN_X),
                        round_fl_to_int(centy - NODE_DY * 0.5f),
                        (short)(BLI_rctf_size_x(rct) - 18.0f - 12.0f),
                        (short)NODE_DY,
                        NULL,
                        0,
                        0,
                        0,
                        0,
                        "");
  if (node->flag & NODE_MUTED) {
    UI_but_flag_enable(but, UI_BUT_INACTIVE);
  }

  /* scale widget thing */
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformThemeColorShade(color_id, -10);
  dx = 10.0f;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct->xmax - dx, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx, centy + 4.0f);

  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->aspect, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->aspect, centy + 4.0f);
  immEnd();

  immUniformThemeColorShade(color_id, 30);
  dx -= snode->aspect;

  immBegin(GPU_PRIM_LINES, 4);
  immVertex2f(pos, rct->xmax - dx, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx, centy + 4.0f);

  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->aspect, centy - 4.0f);
  immVertex2f(pos, rct->xmax - dx - 3.0f * snode->aspect, centy + 4.0f);
  immEnd();

  immUnbindProgram();

  node_draw_sockets(v2d, C, ntree, node, true, false);

  UI_block_end(C, node->block);
  UI_block_draw(C, node->block);
  node->block = NULL;
}

int node_get_resize_cursor(int directions)
{
  if (directions == 0) {
    return CURSOR_STD;
  }
  else if ((directions & ~(NODE_RESIZE_TOP | NODE_RESIZE_BOTTOM)) == 0) {
    return CURSOR_Y_MOVE;
  }
  else if ((directions & ~(NODE_RESIZE_RIGHT | NODE_RESIZE_LEFT)) == 0) {
    return CURSOR_X_MOVE;
  }
  else {
    return CURSOR_EDIT;
  }
}

void node_set_cursor(wmWindow *win, SpaceNode *snode, float cursor[2])
{
  bNodeTree *ntree = snode->edittree;
  bNode *node;
  bNodeSocket *sock;
  int wmcursor = CURSOR_STD;

  if (ntree) {
    if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_IN | SOCK_OUT)) {
      /* pass */
    }
    else {
      /* check nodes front to back */
      for (node = ntree->nodes.last; node; node = node->prev) {
        if (BLI_rctf_isect_pt(&node->totr, cursor[0], cursor[1])) {
          break; /* first hit on node stops */
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
                       ARegion *ar,
                       SpaceNode *snode,
                       bNodeTree *ntree,
                       bNode *node,
                       bNodeInstanceKey key)
{
  if (node->flag & NODE_HIDDEN) {
    node_draw_hidden(C, ar, snode, ntree, node, key);
  }
  else {
    node_draw_basis(C, ar, snode, ntree, node, key);
  }
}

static void node_update(const bContext *C, bNodeTree *ntree, bNode *node)
{
  if (node->typeinfo->draw_nodetype_prepare) {
    node->typeinfo->draw_nodetype_prepare(C, ntree, node);
  }
}

void node_update_nodetree(const bContext *C, bNodeTree *ntree)
{
  bNode *node;

  /* make sure socket "used" tags are correct, for displaying value buttons */
  ntreeTagUsedSockets(ntree);

  /* update nodes front to back, so children sizes get updated before parents */
  for (node = ntree->nodes.last; node; node = node->prev) {
    node_update(C, ntree, node);
  }
}

static void node_draw(const bContext *C,
                      ARegion *ar,
                      SpaceNode *snode,
                      bNodeTree *ntree,
                      bNode *node,
                      bNodeInstanceKey key)
{
  if (node->typeinfo->draw_nodetype) {
    node->typeinfo->draw_nodetype(C, ar, snode, ntree, node, key);
  }
}

#define USE_DRAW_TOT_UPDATE

void node_draw_nodetree(const bContext *C,
                        ARegion *ar,
                        SpaceNode *snode,
                        bNodeTree *ntree,
                        bNodeInstanceKey parent_key)
{
  bNode *node;
  bNodeLink *link;
  int a;

  if (ntree == NULL) {
    return; /* groups... */
  }

#ifdef USE_DRAW_TOT_UPDATE
  if (ntree->nodes.first) {
    BLI_rctf_init_minmax(&ar->v2d.tot);
  }
#endif

  /* draw background nodes, last nodes in front */
  for (a = 0, node = ntree->nodes.first; node; node = node->next, a++) {
    bNodeInstanceKey key;

#ifdef USE_DRAW_TOT_UPDATE
    /* unrelated to background nodes, update the v2d->tot,
     * can be anywhere before we draw the scroll bars */
    BLI_rctf_union(&ar->v2d.tot, &node->totr);
#endif

    if (!(node->flag & NODE_BACKGROUND)) {
      continue;
    }

    key = BKE_node_instance_key(parent_key, ntree, node);
    node->nr = a; /* index of node in list, used for exec event code */
    node_draw(C, ar, snode, ntree, node, key);
  }

  /* node lines */
  GPU_blend(true);
  nodelink_batch_start(snode);
  for (link = ntree->links.first; link; link = link->next) {
    if (!nodeLinkIsHidden(link)) {
      node_draw_link(&ar->v2d, snode, link);
    }
  }
  nodelink_batch_end(snode);
  GPU_blend(false);

  /* draw foreground nodes, last nodes in front */
  for (a = 0, node = ntree->nodes.first; node; node = node->next, a++) {
    bNodeInstanceKey key;
    if (node->flag & NODE_BACKGROUND) {
      continue;
    }

    key = BKE_node_instance_key(parent_key, ntree, node);
    node->nr = a; /* index of node in list, used for exec event code */
    node_draw(C, ar, snode, ntree, node, key);
  }
}

/* draw tree path info in lower left corner */
static void draw_tree_path(SpaceNode *snode)
{
  char info[256];

  ED_node_tree_path_get_fixedbuf(snode, info, sizeof(info));

  UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
  BLF_draw_default(1.5f * UI_UNIT_X, 1.5f * UI_UNIT_Y, 0.0f, info, sizeof(info));
}

static void snode_setup_v2d(SpaceNode *snode, ARegion *ar, const float center[2])
{
  View2D *v2d = &ar->v2d;

  /* shift view to node tree center */
  UI_view2d_center_set(v2d, center[0], center[1]);
  UI_view2d_view_ortho(v2d);

  /* aspect+font, set each time */
  snode->aspect = BLI_rctf_size_x(&v2d->cur) / (float)ar->winx;
  // XXX snode->curfont = uiSetCurFont_ext(snode->aspect);
}

static void draw_nodetree(const bContext *C,
                          ARegion *ar,
                          bNodeTree *ntree,
                          bNodeInstanceKey parent_key)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  node_uiblocks_init(C, ntree);

  node_update_nodetree(C, ntree);
  node_draw_nodetree(C, ar, snode, ntree, parent_key);
}

/* shade the parent node group and add a uiBlock to clip mouse events */
static void draw_group_overlay(const bContext *C, ARegion *ar)
{
  View2D *v2d = &ar->v2d;
  rctf rect = v2d->cur;
  uiBlock *block;
  float color[4];

  /* shade node groups to separate them visually */
  GPU_blend(true);

  UI_GetThemeColorShadeAlpha4fv(TH_NODE_GROUP, 0, 0, color);
  UI_draw_roundbox_corner_set(UI_CNR_NONE);
  UI_draw_roundbox_4fv(true, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 0, color);
  GPU_blend(false);

  /* set the block bounds to clip mouse events from underlying nodes */
  block = UI_block_begin(C, ar, "node tree bounds block", UI_EMBOSS);
  UI_block_bounds_set_explicit(block, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  UI_block_flag_enable(block, UI_BLOCK_CLIP_EVENTS);
  UI_block_end(C, block);
}

void drawnodespace(const bContext *C, ARegion *ar)
{
  wmWindow *win = CTX_wm_window(C);
  View2DScrollers *scrollers;
  SpaceNode *snode = CTX_wm_space_node(C);
  View2D *v2d = &ar->v2d;

  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* XXX snode->cursor set in coordspace for placing new nodes, used for drawing noodles too */
  UI_view2d_region_to_view(&ar->v2d,
                           win->eventstate->x - ar->winrct.xmin,
                           win->eventstate->y - ar->winrct.ymin,
                           &snode->cursor[0],
                           &snode->cursor[1]);
  snode->cursor[0] /= UI_DPI_FAC;
  snode->cursor[1] /= UI_DPI_FAC;

  ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

  /* only set once */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  /* nodes */
  snode_set_context(C);

  /* draw parent node trees */
  if (snode->treepath.last) {
    static const int max_depth = 2;
    bNodeTreePath *path;
    int depth, curdepth;
    float center[2];
    bNodeTree *ntree;
    bNodeLinkDrag *nldrag;
    LinkData *linkdata;

    path = snode->treepath.last;

    /* update tree path name (drawn in the bottom left) */
    ID *name_id = (path->nodetree && path->nodetree != snode->nodetree) ? &path->nodetree->id :
                                                                          snode->id;

    if (name_id && UNLIKELY(!STREQ(path->node_name, name_id->name + 2))) {
      BLI_strncpy(path->node_name, name_id->name + 2, sizeof(path->node_name));
    }

    /* current View2D center, will be set temporarily for parent node trees */
    UI_view2d_center_get(v2d, &center[0], &center[1]);

    /* store new view center in path and current edittree */
    copy_v2_v2(path->view_center, center);
    if (snode->edittree) {
      copy_v2_v2(snode->edittree->view_center, center);
    }

    depth = 0;
    while (path->prev && depth < max_depth) {
      path = path->prev;
      ++depth;
    }

    /* parent node trees in the background */
    for (curdepth = depth; curdepth > 0; path = path->next, --curdepth) {
      ntree = path->nodetree;
      if (ntree) {
        snode_setup_v2d(snode, ar, path->view_center);

        draw_nodetree(C, ar, ntree, path->parent_key);

        draw_group_overlay(C, ar);
      }
    }

    /* top-level edit tree */
    ntree = path->nodetree;
    if (ntree) {
      snode_setup_v2d(snode, ar, center);

      /* grid, uses theme color based on node path depth */
      UI_view2d_multi_grid_draw(
          v2d, (depth > 0 ? TH_NODE_GROUP : TH_BACK), ED_node_grid_size(), NODE_GRID_STEPS, 2);

      /* backdrop */
      draw_nodespace_back_pix(C, ar, snode, path->parent_key);

      {
        float original_proj[4][4];
        GPU_matrix_projection_get(original_proj);

        GPU_matrix_push();
        GPU_matrix_identity_set();

        wmOrtho2_pixelspace(ar->winx, ar->winy);

        WM_gizmomap_draw(ar->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);

        GPU_matrix_pop();
        GPU_matrix_projection_set(original_proj);
      }

      draw_nodetree(C, ar, ntree, path->parent_key);
    }

    /* temporary links */
    GPU_blend(true);
    GPU_line_smooth(true);
    for (nldrag = snode->linkdrag.first; nldrag; nldrag = nldrag->next) {
      for (linkdata = nldrag->links.first; linkdata; linkdata = linkdata->next) {
        node_draw_link(v2d, snode, (bNodeLink *)linkdata->data);
      }
    }
    GPU_line_smooth(false);
    GPU_blend(false);

    if (snode->flag & SNODE_SHOW_GPENCIL) {
      /* draw grease-pencil ('canvas' strokes) */
      ED_annotation_draw_view2d(C, true);
    }
  }
  else {
    /* default grid */
    UI_view2d_multi_grid_draw(v2d, TH_BACK, ED_node_grid_size(), NODE_GRID_STEPS, 2);

    /* backdrop */
    draw_nodespace_back_pix(C, ar, snode, NODE_INSTANCE_KEY_NONE);
  }

  ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  if (snode->treepath.last) {
    if (snode->flag & SNODE_SHOW_GPENCIL) {
      /* draw grease-pencil (screen strokes, and also paintbuffer) */
      ED_annotation_draw_view2d(C, false);
    }
  }

  /* tree path info */
  draw_tree_path(snode);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}
