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
 * \ingroup spnode
 * \brief lower level node drawing for nodes (boarders, headers etc), also node layout.
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_system.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "BIF_glutil.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_node.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "NOD_composite.h"
#include "NOD_shader.h"
#include "NOD_simulation.h"
#include "NOD_texture.h"
#include "node_intern.h" /* own include */

/* Default flags for uiItemR(). Name is kept short since this is used a lot in this file. */
#define DEFAULT_FLAGS UI_ITEM_R_SPLIT_EMPTY_NAME

/* ****************** SOCKET BUTTON DRAW FUNCTIONS ***************** */

static void node_socket_button_label(bContext *UNUSED(C),
                                     uiLayout *layout,
                                     PointerRNA *UNUSED(ptr),
                                     PointerRNA *UNUSED(node_ptr),
                                     const char *text)
{
  uiItemL(layout, text, 0);
}

/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

static void node_buts_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = ptr->data;
  /* first output stores value */
  bNodeSocket *output = node->outputs.first;
  PointerRNA sockptr;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, output, &sockptr);

  uiItemR(layout, &sockptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_rgb(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = ptr->data;
  /* first output stores value */
  bNodeSocket *output = node->outputs.first;
  PointerRNA sockptr;
  uiLayout *col;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, output, &sockptr);

  col = uiLayoutColumn(layout, false);
  uiTemplateColorPicker(col, &sockptr, "default_value", 1, 0, 0, 0);
  uiItemR(col, &sockptr, "default_value", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
}

static void node_buts_mix_rgb(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row, *col;

  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "blend_type", DEFAULT_FLAGS, "", ICON_NONE);
  if (ELEM(ntree->type, NTREE_COMPOSIT, NTREE_TEXTURE)) {
    uiItemR(row, ptr, "use_alpha", DEFAULT_FLAGS, "", ICON_IMAGE_RGB_ALPHA);
  }

  uiItemR(col, ptr, "use_clamp", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_buts_time(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row;
#if 0
  /* XXX no context access here .. */
  bNode *node = ptr->data;
  CurveMapping *cumap = node->storage;

  if (cumap) {
    cumap->flag |= CUMA_DRAW_CFRA;
    if (node->custom1 < node->custom2) {
      cumap->sample[0] = (float)(CFRA - node->custom1) / (float)(node->custom2 - node->custom1);
    }
  }
#endif

  uiTemplateCurveMapping(layout, ptr, "curve", 's', false, false, false, false);

  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "frame_start", DEFAULT_FLAGS, IFACE_("Sta"), ICON_NONE);
  uiItemR(row, ptr, "frame_end", DEFAULT_FLAGS, IFACE_("End"), ICON_NONE);
}

static void node_buts_colorramp(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiTemplateColorRamp(layout, ptr, "color_ramp", 0);
}

static void node_buts_curvevec(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 'v', false, false, false, false);
}

#define SAMPLE_FLT_ISNONE FLT_MAX
/* bad bad, 2.5 will do better?... no it won't... */
static float _sample_col[4] = {SAMPLE_FLT_ISNONE};
void ED_node_sample_set(const float col[4])
{
  if (col) {
    copy_v4_v4(_sample_col, col);
  }
  else {
    copy_v4_fl(_sample_col, SAMPLE_FLT_ISNONE);
  }
}

static void node_buts_curvecol(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = ptr->data;
  CurveMapping *cumap = node->storage;

  if (_sample_col[0] != SAMPLE_FLT_ISNONE) {
    cumap->flag |= CUMA_DRAW_SAMPLE;
    copy_v3_v3(cumap->sample, _sample_col);
  }
  else {
    cumap->flag &= ~CUMA_DRAW_SAMPLE;
  }

  uiTemplateCurveMapping(layout, ptr, "mapping", 'c', false, false, false, true);
}

static void node_buts_normal(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = ptr->data;
  /* first output stores normal */
  bNodeSocket *output = node->outputs.first;
  PointerRNA sockptr;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, output, &sockptr);

  uiItemR(layout, &sockptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
}

#if 0 /* not used in 2.5x yet */
static void node_browse_tex_cb(bContext *C, void *ntree_v, void *node_v)
{
  Main *bmain = CTX_data_main(C);
  bNodeTree *ntree = ntree_v;
  bNode *node = node_v;
  Tex *tex;

  if (node->menunr < 1) {
    return;
  }

  if (node->id) {
    id_us_min(node->id);
    node->id = NULL;
  }
  tex = BLI_findlink(&bmain->tex, node->menunr - 1);

  node->id = &tex->id;
  id_us_plus(node->id);
  BLI_strncpy(node->name, node->id->name + 2, sizeof(node->name));

  nodeSetActive(ntree, node);

  if (ntree->type == NTREE_TEXTURE) {
    ntreeTexCheckCyclics(ntree);
  }

  // allqueue(REDRAWBUTSSHADING, 0);
  // allqueue(REDRAWNODE, 0);
  NodeTagChanged(ntree, node);

  node->menunr = 0;
}
#endif

static void node_buts_texture(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = ptr->data;

  short multi = (node->id && ((Tex *)node->id)->use_nodes && (node->type != CMP_NODE_TEXTURE) &&
                 (node->type != TEX_NODE_TEXTURE));

  uiItemR(layout, ptr, "texture", DEFAULT_FLAGS, "", ICON_NONE);

  if (multi) {
    /* Number Drawing not optimal here, better have a list*/
    uiItemR(layout, ptr, "node_output", DEFAULT_FLAGS, "", ICON_NONE);
  }
}

static void node_shader_buts_clamp(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "clamp_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_map_range(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "interpolation_type", DEFAULT_FLAGS, "", ICON_NONE);
  if (!ELEM(RNA_enum_get(ptr, "interpolation_type"),
            NODE_MAP_RANGE_SMOOTHSTEP,
            NODE_MAP_RANGE_SMOOTHERSTEP)) {
    uiItemR(layout, ptr, "clamp", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_buts_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "use_clamp", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static int node_resize_area_default(bNode *node, int x, int y)
{
  if (node->flag & NODE_HIDDEN) {
    rctf totr = node->totr;
    /* right part of node */
    totr.xmin = node->totr.xmax - 20.0f;
    if (BLI_rctf_isect_pt(&totr, x, y)) {
      return NODE_RESIZE_RIGHT;
    }
    else {
      return 0;
    }
  }
  else {
    const float size = NODE_RESIZE_MARGIN;
    rctf totr = node->totr;
    int dir = 0;

    if (x >= totr.xmax - size && x < totr.xmax && y >= totr.ymin && y < totr.ymax) {
      dir |= NODE_RESIZE_RIGHT;
    }
    if (x >= totr.xmin && x < totr.xmin + size && y >= totr.ymin && y < totr.ymax) {
      dir |= NODE_RESIZE_LEFT;
    }
    return dir;
  }
}

/* ****************** BUTTON CALLBACKS FOR COMMON NODES ***************** */

static void node_draw_buttons_group(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateIDBrowse(
      layout, C, ptr, "node_tree", NULL, NULL, NULL, UI_TEMPLATE_ID_FILTER_ALL, NULL);
}

/* XXX Does a bounding box update by iterating over all children.
 * Not ideal to do this in every draw call, but doing as transform callback doesn't work,
 * since the child node totr rects are not updated properly at that point.
 */
static void node_draw_frame_prepare(const bContext *UNUSED(C), bNodeTree *ntree, bNode *node)
{
  const float margin = 1.5f * U.widget_unit;
  NodeFrame *data = (NodeFrame *)node->storage;
  bool bbinit;
  bNode *tnode;
  rctf rect, noderect;
  float xmax, ymax;

  /* init rect from current frame size */
  node_to_view(node, node->offsetx, node->offsety, &rect.xmin, &rect.ymax);
  node_to_view(
      node, node->offsetx + node->width, node->offsety - node->height, &rect.xmax, &rect.ymin);

  /* frame can be resized manually only if shrinking is disabled or no children are attached */
  data->flag |= NODE_FRAME_RESIZEABLE;
  /* for shrinking bbox, initialize the rect from first child node */
  bbinit = (data->flag & NODE_FRAME_SHRINK);
  /* fit bounding box to all children */
  for (tnode = ntree->nodes.first; tnode; tnode = tnode->next) {
    if (tnode->parent != node) {
      continue;
    }

    /* add margin to node rect */
    noderect = tnode->totr;
    noderect.xmin -= margin;
    noderect.xmax += margin;
    noderect.ymin -= margin;
    noderect.ymax += margin;

    /* first child initializes frame */
    if (bbinit) {
      bbinit = 0;
      rect = noderect;
      data->flag &= ~NODE_FRAME_RESIZEABLE;
    }
    else {
      BLI_rctf_union(&rect, &noderect);
    }
  }

  /* now adjust the frame size from view-space bounding box */
  node_from_view(node, rect.xmin, rect.ymax, &node->offsetx, &node->offsety);
  node_from_view(node, rect.xmax, rect.ymin, &xmax, &ymax);
  node->width = xmax - node->offsetx;
  node->height = -ymax + node->offsety;

  node->totr = rect;
}

static void node_draw_frame_label(bNodeTree *ntree, bNode *node, const float aspect)
{
  /* XXX font id is crap design */
  const int fontid = UI_style_get()->widgetlabel.uifont_id;
  NodeFrame *data = (NodeFrame *)node->storage;
  rctf *rct = &node->totr;
  int color_id = node_get_colorid(node);
  char label[MAX_NAME];
  /* XXX a bit hacky, should use separate align values for x and y */
  float width, ascender;
  float x, y;
  const int font_size = data->label_size / aspect;
  const float margin = (float)(NODE_DY / 4);
  int label_height;
  uchar color[3];

  nodeLabel(ntree, node, label, sizeof(label));

  BLF_enable(fontid, BLF_ASPECT);
  BLF_aspect(fontid, aspect, aspect, 1.0f);
  /* clamp otherwise it can suck up a LOT of memory */
  BLF_size(fontid, MIN2(24, font_size), U.dpi);

  /* title color */
  UI_GetThemeColorBlendShade3ubv(TH_TEXT, color_id, 0.4f, 10, color);
  BLF_color3ubv(fontid, color);

  width = BLF_width(fontid, label, sizeof(label));
  ascender = BLF_ascender(fontid);
  label_height = ((margin / aspect) + (ascender * aspect));

  /* 'x' doesn't need aspect correction */
  x = BLI_rctf_cent_x(rct) - (0.5f * width);
  y = rct->ymax - label_height;

  BLF_position(fontid, x, y, 0);
  BLF_draw(fontid, label, BLF_DRAW_STR_DUMMY_MAX);

  /* draw text body */
  if (node->id) {
    Text *text = (Text *)node->id;
    TextLine *line;
    const int line_height_max = BLF_height_max(fontid);
    const float line_spacing = (line_height_max * aspect);
    const float line_width = (BLI_rctf_size_x(rct) - margin) / aspect;
    int y_min;

    /* 'x' doesn't need aspect correction */
    x = rct->xmin + margin;
    y = rct->ymax - (label_height + line_spacing);
    /* early exit */
    y_min = y + ((margin * 2) - (y - rct->ymin));

    BLF_enable(fontid, BLF_CLIPPING | BLF_WORD_WRAP);
    BLF_clipping(fontid,
                 rct->xmin,
                 /* round to avoid clipping half-way through a line */
                 y - (floorf(((y - rct->ymin) - (margin * 2)) / line_spacing) * line_spacing),
                 rct->xmin + line_width,
                 rct->ymax);

    BLF_wordwrap(fontid, line_width);

    for (line = text->lines.first; line; line = line->next) {
      struct ResultBLF info;
      if (line->line[0]) {
        BLF_position(fontid, x, y, 0);
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

static void node_draw_frame(const bContext *C,
                            ARegion *region,
                            SpaceNode *snode,
                            bNodeTree *ntree,
                            bNode *node,
                            bNodeInstanceKey UNUSED(key))
{
  rctf *rct = &node->totr;
  int color_id = node_get_colorid(node);
  float color[4];
  float alpha;

  /* skip if out of view */
  if (BLI_rctf_isect(&node->totr, &region->v2d.cur, NULL) == false) {
    UI_block_end(C, node->block);
    node->block = NULL;
    return;
  }

  UI_GetThemeColor4fv(TH_NODE_FRAME, color);
  alpha = color[3];

  /* shadow */
  node_draw_shadow(snode, node, BASIS_RAD, alpha);

  /* body */
  if (node->flag & NODE_CUSTOM_COLOR) {
    rgba_float_args_set(color, node->color[0], node->color[1], node->color[2], alpha);
  }
  else {
    UI_GetThemeColor4fv(TH_NODE_FRAME, color);
  }

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_aa(true, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD, color);

  /* outline active and selected emphasis */
  if (node->flag & SELECT) {
    if (node->flag & NODE_ACTIVE) {
      UI_GetThemeColorShadeAlpha4fv(TH_ACTIVE, 0, -40, color);
    }
    else {
      UI_GetThemeColorShadeAlpha4fv(TH_SELECT, 0, -40, color);
    }

    UI_draw_roundbox_aa(false, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD, color);
  }

  /* label */
  node_draw_frame_label(ntree, node, snode->aspect);

  UI_ThemeClearColor(color_id);

  UI_block_end(C, node->block);
  UI_block_draw(C, node->block);
  node->block = NULL;
}

static int node_resize_area_frame(bNode *node, int x, int y)
{
  const float size = 10.0f;
  NodeFrame *data = (NodeFrame *)node->storage;
  rctf totr = node->totr;
  int dir = 0;

  /* shrinking frame size is determined by child nodes */
  if (!(data->flag & NODE_FRAME_RESIZEABLE)) {
    return 0;
  }

  if (x >= totr.xmax - size && x < totr.xmax && y >= totr.ymin && y < totr.ymax) {
    dir |= NODE_RESIZE_RIGHT;
  }
  if (x >= totr.xmin && x < totr.xmin + size && y >= totr.ymin && y < totr.ymax) {
    dir |= NODE_RESIZE_LEFT;
  }
  if (x >= totr.xmin && x < totr.xmax && y >= totr.ymax - size && y < totr.ymax) {
    dir |= NODE_RESIZE_TOP;
  }
  if (x >= totr.xmin && x < totr.xmax && y >= totr.ymin && y < totr.ymin + size) {
    dir |= NODE_RESIZE_BOTTOM;
  }

  return dir;
}

static void node_buts_frame_ex(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "label_size", DEFAULT_FLAGS, IFACE_("Label Size"), ICON_NONE);
  uiItemR(layout, ptr, "shrink", DEFAULT_FLAGS, IFACE_("Shrink"), ICON_NONE);
  uiItemR(layout, ptr, "text", DEFAULT_FLAGS, NULL, ICON_NONE);
}

#define NODE_REROUTE_SIZE 8.0f

static void node_draw_reroute_prepare(const bContext *UNUSED(C),
                                      bNodeTree *UNUSED(ntree),
                                      bNode *node)
{
  bNodeSocket *nsock;
  float locx, locy;
  float size = NODE_REROUTE_SIZE;

  /* get "global" coords */
  node_to_view(node, 0.0f, 0.0f, &locx, &locy);

  /* reroute node has exactly one input and one output, both in the same place */
  nsock = node->outputs.first;
  nsock->locx = locx;
  nsock->locy = locy;

  nsock = node->inputs.first;
  nsock->locx = locx;
  nsock->locy = locy;

  node->width = size * 2;
  node->totr.xmin = locx - size;
  node->totr.xmax = locx + size;
  node->totr.ymax = locy + size;
  node->totr.ymin = locy - size;
}

static void node_draw_reroute(const bContext *C,
                              ARegion *region,
                              SpaceNode *UNUSED(snode),
                              bNodeTree *ntree,
                              bNode *node,
                              bNodeInstanceKey UNUSED(key))
{
  char showname[128]; /* 128 used below */
  rctf *rct = &node->totr;

  /* skip if out of view */
  if (node->totr.xmax < region->v2d.cur.xmin || node->totr.xmin > region->v2d.cur.xmax ||
      node->totr.ymax < region->v2d.cur.ymin || node->totr.ymin > region->v2d.cur.ymax) {
    UI_block_end(C, node->block);
    node->block = NULL;
    return;
  }

  /* XXX only kept for debugging
   * selection state is indicated by socket outline below!
   */
#if 0
  float size = NODE_REROUTE_SIZE;

  /* body */
  float debug_color[4];
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_GetThemeColor4fv(TH_NODE, debug_color);
  UI_draw_roundbox_aa(true, rct->xmin, rct->ymin, rct->xmax, rct->ymax, size, debug_color);

  /* outline active and selected emphasis */
  if (node->flag & SELECT) {
    GPU_blend(true);
    GPU_line_smooth(true);
    /* using different shades of TH_TEXT_HI for the empasis, like triangle */
    if (node->flag & NODE_ACTIVE) {
      UI_GetThemeColorShadeAlpha4fv(TH_TEXT_HI, 0, -40, debug_color);
    }
    else {
      UI_GetThemeColorShadeAlpha4fv(TH_TEXT_HI, -20, -120, debug_color);
    }
    UI_draw_roundbox_4fv(false, rct->xmin, rct->ymin, rct->xmax, rct->ymax, size, debug_color);

    GPU_line_smooth(false);
    GPU_blend(false);
  }
#endif

  if (node->label[0] != '\0') {
    /* draw title (node label) */
    BLI_strncpy(showname, node->label, sizeof(showname));
    uiDefBut(node->block,
             UI_BTYPE_LABEL,
             0,
             showname,
             (int)(rct->xmin - NODE_DYS),
             (int)(rct->ymax),
             (short)512,
             (short)NODE_DY,
             NULL,
             0,
             0,
             0,
             0,
             NULL);
  }

  /* only draw input socket. as they all are placed on the same position.
   * highlight also if node itself is selected, since we don't display the node body separately!
   */
  node_draw_sockets(&region->v2d, C, ntree, node, false, node->flag & SELECT);

  UI_block_end(C, node->block);
  UI_block_draw(C, node->block);
  node->block = NULL;
}

/* Special tweak area for reroute node.
 * Since this node is quite small, we use a larger tweak area for grabbing than for selection.
 */
static int node_tweak_area_reroute(bNode *node, int x, int y)
{
  /* square of tweak radius */
  const float tweak_radius_sq = square_f(24.0f);

  bNodeSocket *sock = node->inputs.first;
  float dx = sock->locx - x;
  float dy = sock->locy - y;
  return (dx * dx + dy * dy <= tweak_radius_sq);
}

static void node_common_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case NODE_GROUP:
      ntype->draw_buttons = node_draw_buttons_group;
      break;
    case NODE_FRAME:
      ntype->draw_nodetype = node_draw_frame;
      ntype->draw_nodetype_prepare = node_draw_frame_prepare;
      ntype->draw_buttons_ex = node_buts_frame_ex;
      ntype->resize_area_func = node_resize_area_frame;
      break;
    case NODE_REROUTE:
      ntype->draw_nodetype = node_draw_reroute;
      ntype->draw_nodetype_prepare = node_draw_reroute_prepare;
      ntype->tweak_area_func = node_tweak_area_reroute;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR SHADER NODES ***************** */

static void node_buts_image_user(uiLayout *layout,
                                 bContext *C,
                                 PointerRNA *ptr,
                                 PointerRNA *imaptr,
                                 PointerRNA *iuserptr,
                                 bool compositor)
{
  uiLayout *col;
  int source;

  if (!imaptr->data) {
    return;
  }

  col = uiLayoutColumn(layout, false);

  uiItemR(col, imaptr, "source", DEFAULT_FLAGS, "", ICON_NONE);

  source = RNA_enum_get(imaptr, "source");

  if (source == IMA_SRC_SEQUENCE) {
    /* don't use iuser->framenr directly
     * because it may not be updated if auto-refresh is off */
    Scene *scene = CTX_data_scene(C);
    ImageUser *iuser = iuserptr->data;
    /* Image *ima = imaptr->data; */ /* UNUSED */

    char numstr[32];
    const int framenr = BKE_image_user_frame_get(iuser, CFRA, NULL);
    BLI_snprintf(numstr, sizeof(numstr), IFACE_("Frame: %d"), framenr);
    uiItemL(layout, numstr, ICON_NONE);
  }

  if (ELEM(source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "frame_duration", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "frame_start", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "frame_offset", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "use_cyclic", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "use_auto_refresh", DEFAULT_FLAGS, NULL, ICON_NONE);
  }

  if (compositor && RNA_enum_get(imaptr, "type") == IMA_TYPE_MULTILAYER &&
      RNA_boolean_get(ptr, "has_layers")) {
    col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "layer", DEFAULT_FLAGS, NULL, ICON_NONE);
  }

  uiLayout *split = uiLayoutSplit(layout, 0.5f, true);
  PointerRNA colorspace_settings_ptr = RNA_pointer_get(imaptr, "colorspace_settings");
  uiItemL(split, IFACE_("Color Space"), ICON_NONE);
  uiItemR(split, &colorspace_settings_ptr, "name", DEFAULT_FLAGS, "", ICON_NONE);

  /* Avoid losing changes image is painted. */
  if (BKE_image_is_dirty(imaptr->data)) {
    uiLayoutSetEnabled(split, false);
  }
}

static void node_shader_buts_mapping(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "vector_type", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_shader_buts_vector_rotate(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "rotation_type", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "invert", DEFAULT_FLAGS, NULL, 0);
}

static void node_shader_buts_vect_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_vect_transform(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "vector_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  uiItemR(layout, ptr, "convert_from", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "convert_to", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_attribute(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "attribute_name", DEFAULT_FLAGS, IFACE_("Name"), ICON_NONE);
}

static void node_shader_buts_wireframe(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_pixel_size", DEFAULT_FLAGS, NULL, 0);
}

static void node_shader_buts_tex_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA imaptr = RNA_pointer_get(ptr, "image");
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");

  uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
  uiTemplateID(layout,
               C,
               ptr,
               "image",
               "IMAGE_OT_new",
               "IMAGE_OT_open",
               NULL,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               NULL);
  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "projection", DEFAULT_FLAGS, "", ICON_NONE);

  if (RNA_enum_get(ptr, "projection") == SHD_PROJ_BOX) {
    uiItemR(layout, ptr, "projection_blend", DEFAULT_FLAGS, "Blend", ICON_NONE);
  }

  uiItemR(layout, ptr, "extension", DEFAULT_FLAGS, "", ICON_NONE);

  /* note: image user properties used directly here, unlike compositor image node,
   * which redefines them in the node struct RNA to get proper updates.
   */
  node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr, false);
}

static void node_shader_buts_tex_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0, 0);
}

static void node_shader_buts_tex_environment(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA imaptr = RNA_pointer_get(ptr, "image");
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");

  uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
  uiTemplateID(layout,
               C,
               ptr,
               "image",
               "IMAGE_OT_new",
               "IMAGE_OT_open",
               NULL,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               NULL);

  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "projection", DEFAULT_FLAGS, "", ICON_NONE);

  node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr, false);
}

static void node_shader_buts_tex_environment_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0, 0);

  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, IFACE_("Interpolation"), ICON_NONE);
  uiItemR(layout, ptr, "projection", DEFAULT_FLAGS, IFACE_("Projection"), ICON_NONE);
}

static void node_shader_buts_tex_sky(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "sky_type", DEFAULT_FLAGS, "", ICON_NONE);

  if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_PREETHAM) {
    uiItemR(layout, ptr, "sun_direction", DEFAULT_FLAGS, "", ICON_NONE);
    uiItemR(layout, ptr, "turbidity", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_HOSEK) {
    uiItemR(layout, ptr, "sun_direction", DEFAULT_FLAGS, "", ICON_NONE);
    uiItemR(layout, ptr, "turbidity", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(layout, ptr, "ground_albedo", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  if (RNA_enum_get(ptr, "sky_type") == SHD_SKY_NISHITA) {
    uiItemR(layout, ptr, "sun_disc", DEFAULT_FLAGS, NULL, 0);

    if (RNA_boolean_get(ptr, "sun_disc")) {
      uiItemR(layout, ptr, "sun_size", DEFAULT_FLAGS, NULL, ICON_NONE);
    }

    uiLayout *col;
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "sun_elevation", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "sun_rotation", DEFAULT_FLAGS, NULL, ICON_NONE);

    uiItemR(layout, ptr, "altitude", DEFAULT_FLAGS, NULL, ICON_NONE);

    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "air_density", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "dust_density", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "ozone_density", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_shader_buts_tex_gradient(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "gradient_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_tex_magic(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "turbulence_depth", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_shader_buts_tex_brick(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "offset", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
  uiItemR(col, ptr, "offset_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "squash", DEFAULT_FLAGS, IFACE_("Squash"), ICON_NONE);
  uiItemR(col, ptr, "squash_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);
}

static void node_shader_buts_tex_wave(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "wave_type", DEFAULT_FLAGS, "", ICON_NONE);
  int type = RNA_enum_get(ptr, "wave_type");
  if (type == SHD_WAVE_BANDS) {
    uiItemR(layout, ptr, "bands_direction", DEFAULT_FLAGS, "", ICON_NONE);
  }
  else { /* SHD_WAVE_RINGS */
    uiItemR(layout, ptr, "rings_direction", DEFAULT_FLAGS, "", ICON_NONE);
  }

  uiItemR(layout, ptr, "wave_profile", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_tex_musgrave(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "musgrave_dimensions", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "musgrave_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_tex_voronoi(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "voronoi_dimensions", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "feature", DEFAULT_FLAGS, "", ICON_NONE);
  int feature = RNA_enum_get(ptr, "feature");
  if (!ELEM(feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS) &&
      RNA_enum_get(ptr, "voronoi_dimensions") != 1) {
    uiItemR(layout, ptr, "distance", DEFAULT_FLAGS, "", ICON_NONE);
  }
}

static void node_shader_buts_tex_noise(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "noise_dimensions", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_tex_pointdensity(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  bNode *node = ptr->data;
  NodeShaderTexPointDensity *shader_point_density = node->storage;
  Object *ob = (Object *)node->id;
  PointerRNA ob_ptr, obdata_ptr;

  RNA_id_pointer_create((ID *)ob, &ob_ptr);
  RNA_id_pointer_create(ob ? (ID *)ob->data : NULL, &obdata_ptr);

  uiItemR(layout, ptr, "point_source", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  uiItemR(layout, ptr, "object", DEFAULT_FLAGS, NULL, ICON_NONE);

  if (node->id && shader_point_density->point_source == SHD_POINTDENSITY_SOURCE_PSYS) {
    PointerRNA dataptr;
    RNA_id_pointer_create((ID *)node->id, &dataptr);
    uiItemPointerR(layout, ptr, "particle_system", &dataptr, "particle_systems", NULL, ICON_NONE);
  }

  uiItemR(layout, ptr, "space", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "radius", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "resolution", DEFAULT_FLAGS, NULL, ICON_NONE);
  if (shader_point_density->point_source == SHD_POINTDENSITY_SOURCE_PSYS) {
    uiItemR(layout, ptr, "particle_color_source", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "vertex_color_source", DEFAULT_FLAGS, NULL, ICON_NONE);
    if (shader_point_density->ob_color_source == SHD_POINTDENSITY_COLOR_VERTWEIGHT) {
      if (ob_ptr.data) {
        uiItemPointerR(
            layout, ptr, "vertex_attribute_name", &ob_ptr, "vertex_groups", "", ICON_NONE);
      }
    }
    if (shader_point_density->ob_color_source == SHD_POINTDENSITY_COLOR_VERTCOL) {
      if (obdata_ptr.data) {
        uiItemPointerR(
            layout, ptr, "vertex_attribute_name", &obdata_ptr, "vertex_colors", "", ICON_NONE);
      }
    }
  }
}

static void node_shader_buts_tex_coord(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "object", DEFAULT_FLAGS, NULL, 0);
  uiItemR(layout, ptr, "from_instancer", DEFAULT_FLAGS, NULL, 0);
}

static void node_shader_buts_bump(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "invert", DEFAULT_FLAGS, NULL, 0);
}

static void node_shader_buts_uvmap(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "from_instancer", DEFAULT_FLAGS, NULL, 0);

  if (!RNA_boolean_get(ptr, "from_instancer")) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

    if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
      PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
      uiItemPointerR(layout, ptr, "uv_map", &dataptr, "uv_layers", "", ICON_NONE);
    }
  }
}

static void node_shader_buts_vertex_color(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA obptr = CTX_data_pointer_get(C, "active_object");
  if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
    PointerRNA dataptr = RNA_pointer_get(&obptr, "data");

    if (RNA_collection_length(&dataptr, "sculpt_vertex_colors")) {
      uiItemPointerR(
          layout, ptr, "layer_name", &dataptr, "sculpt_vertex_colors", "", ICON_GROUP_VCOL);
    }
    else {
      uiItemPointerR(layout, ptr, "layer_name", &dataptr, "vertex_colors", "", ICON_GROUP_VCOL);
    }
  }
  else {
    uiItemL(layout, "No mesh in active object.", ICON_ERROR);
  }
}

static void node_shader_buts_uvalongstroke(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_tips", DEFAULT_FLAGS, NULL, 0);
}

static void node_shader_buts_normal_map(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", DEFAULT_FLAGS, "", 0);

  if (RNA_enum_get(ptr, "space") == SHD_SPACE_TANGENT) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

    if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
      PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
      uiItemPointerR(layout, ptr, "uv_map", &dataptr, "uv_layers", "", ICON_NONE);
    }
    else {
      uiItemR(layout, ptr, "uv_map", DEFAULT_FLAGS, "", 0);
    }
  }
}

static void node_shader_buts_displacement(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", DEFAULT_FLAGS, "", 0);
}

static void node_shader_buts_tangent(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiLayout *split, *row;

  split = uiLayoutSplit(layout, 0.0f, false);

  uiItemR(split, ptr, "direction_type", DEFAULT_FLAGS, "", 0);

  row = uiLayoutRow(split, false);

  if (RNA_enum_get(ptr, "direction_type") == SHD_TANGENT_UVMAP) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

    if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
      PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
      uiItemPointerR(row, ptr, "uv_map", &dataptr, "uv_layers", "", ICON_NONE);
    }
    else {
      uiItemR(row, ptr, "uv_map", DEFAULT_FLAGS, "", 0);
    }
  }
  else {
    uiItemR(row, ptr, "axis", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, 0);
  }
}

static void node_shader_buts_glossy(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_principled(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "subsurface_method", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_anisotropic(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_subsurface(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "falloff", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_toon(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_hair(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_principled_hair(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiItemR(layout, ptr, "parametrization", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_ies(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "mode", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  row = uiLayoutRow(layout, true);

  if (RNA_enum_get(ptr, "mode") == NODE_IES_INTERNAL) {
    uiItemR(row, ptr, "ies", DEFAULT_FLAGS, "", ICON_NONE);
  }
  else {
    uiItemR(row, ptr, "filepath", DEFAULT_FLAGS, "", ICON_NONE);
  }
}

static void node_shader_buts_script(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "mode", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  row = uiLayoutRow(layout, true);

  if (RNA_enum_get(ptr, "mode") == NODE_SCRIPT_INTERNAL) {
    uiItemR(row, ptr, "script", DEFAULT_FLAGS, "", ICON_NONE);
  }
  else {
    uiItemR(row, ptr, "filepath", DEFAULT_FLAGS, "", ICON_NONE);
  }

  uiItemO(row, "", ICON_FILE_REFRESH, "node.shader_script_update");
}

static void node_shader_buts_script_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiItemS(layout);

  node_shader_buts_script(layout, C, ptr);

#if 0 /* not implemented yet */
  if (RNA_enum_get(ptr, "mode") == NODE_SCRIPT_EXTERNAL) {
    uiItemR(layout, ptr, "use_auto_update", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
#endif
}

static void node_buts_output_shader(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "target", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_output_linestyle(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row, *col;

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "blend_type", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(col, ptr, "use_clamp", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_shader_buts_bevel(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "samples", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_shader_buts_ambient_occlusion(uiLayout *layout,
                                               bContext *UNUSED(C),
                                               PointerRNA *ptr)
{
  uiItemR(layout, ptr, "samples", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "inside", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "only_local", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_shader_buts_white_noise(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "noise_dimensions", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_output_aov(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "name", DEFAULT_FLAGS, NULL, ICON_NONE);
}

/* only once called */
static void node_shader_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case SH_NODE_NORMAL:
      ntype->draw_buttons = node_buts_normal;
      break;
    case SH_NODE_CURVE_VEC:
      ntype->draw_buttons = node_buts_curvevec;
      break;
    case SH_NODE_CURVE_RGB:
      ntype->draw_buttons = node_buts_curvecol;
      break;
    case SH_NODE_MAPPING:
      ntype->draw_buttons = node_shader_buts_mapping;
      break;
    case SH_NODE_VALUE:
      ntype->draw_buttons = node_buts_value;
      break;
    case SH_NODE_RGB:
      ntype->draw_buttons = node_buts_rgb;
      break;
    case SH_NODE_MIX_RGB:
      ntype->draw_buttons = node_buts_mix_rgb;
      break;
    case SH_NODE_VALTORGB:
      ntype->draw_buttons = node_buts_colorramp;
      break;
    case SH_NODE_CLAMP:
      ntype->draw_buttons = node_shader_buts_clamp;
      break;
    case SH_NODE_MAP_RANGE:
      ntype->draw_buttons = node_shader_buts_map_range;
      break;
    case SH_NODE_MATH:
      ntype->draw_buttons = node_buts_math;
      break;
    case SH_NODE_VECTOR_MATH:
      ntype->draw_buttons = node_shader_buts_vect_math;
      break;
    case SH_NODE_VECTOR_ROTATE:
      ntype->draw_buttons = node_shader_buts_vector_rotate;
      break;
    case SH_NODE_VECT_TRANSFORM:
      ntype->draw_buttons = node_shader_buts_vect_transform;
      break;
    case SH_NODE_ATTRIBUTE:
      ntype->draw_buttons = node_shader_buts_attribute;
      break;
    case SH_NODE_WIREFRAME:
      ntype->draw_buttons = node_shader_buts_wireframe;
      break;
    case SH_NODE_TEX_SKY:
      ntype->draw_buttons = node_shader_buts_tex_sky;
      break;
    case SH_NODE_TEX_IMAGE:
      ntype->draw_buttons = node_shader_buts_tex_image;
      ntype->draw_buttons_ex = node_shader_buts_tex_image_ex;
      break;
    case SH_NODE_TEX_ENVIRONMENT:
      ntype->draw_buttons = node_shader_buts_tex_environment;
      ntype->draw_buttons_ex = node_shader_buts_tex_environment_ex;
      break;
    case SH_NODE_TEX_GRADIENT:
      ntype->draw_buttons = node_shader_buts_tex_gradient;
      break;
    case SH_NODE_TEX_MAGIC:
      ntype->draw_buttons = node_shader_buts_tex_magic;
      break;
    case SH_NODE_TEX_BRICK:
      ntype->draw_buttons = node_shader_buts_tex_brick;
      break;
    case SH_NODE_TEX_WAVE:
      ntype->draw_buttons = node_shader_buts_tex_wave;
      break;
    case SH_NODE_TEX_MUSGRAVE:
      ntype->draw_buttons = node_shader_buts_tex_musgrave;
      break;
    case SH_NODE_TEX_VORONOI:
      ntype->draw_buttons = node_shader_buts_tex_voronoi;
      break;
    case SH_NODE_TEX_NOISE:
      ntype->draw_buttons = node_shader_buts_tex_noise;
      break;
    case SH_NODE_TEX_POINTDENSITY:
      ntype->draw_buttons = node_shader_buts_tex_pointdensity;
      break;
    case SH_NODE_TEX_COORD:
      ntype->draw_buttons = node_shader_buts_tex_coord;
      break;
    case SH_NODE_BUMP:
      ntype->draw_buttons = node_shader_buts_bump;
      break;
    case SH_NODE_NORMAL_MAP:
      ntype->draw_buttons = node_shader_buts_normal_map;
      break;
    case SH_NODE_DISPLACEMENT:
    case SH_NODE_VECTOR_DISPLACEMENT:
      ntype->draw_buttons = node_shader_buts_displacement;
      break;
    case SH_NODE_TANGENT:
      ntype->draw_buttons = node_shader_buts_tangent;
      break;
    case SH_NODE_BSDF_GLOSSY:
    case SH_NODE_BSDF_GLASS:
    case SH_NODE_BSDF_REFRACTION:
      ntype->draw_buttons = node_shader_buts_glossy;
      break;
    case SH_NODE_BSDF_PRINCIPLED:
      ntype->draw_buttons = node_shader_buts_principled;
      break;
    case SH_NODE_BSDF_ANISOTROPIC:
      ntype->draw_buttons = node_shader_buts_anisotropic;
      break;
    case SH_NODE_SUBSURFACE_SCATTERING:
      ntype->draw_buttons = node_shader_buts_subsurface;
      break;
    case SH_NODE_BSDF_TOON:
      ntype->draw_buttons = node_shader_buts_toon;
      break;
    case SH_NODE_BSDF_HAIR:
      ntype->draw_buttons = node_shader_buts_hair;
      break;
    case SH_NODE_BSDF_HAIR_PRINCIPLED:
      ntype->draw_buttons = node_shader_buts_principled_hair;
      break;
    case SH_NODE_SCRIPT:
      ntype->draw_buttons = node_shader_buts_script;
      ntype->draw_buttons_ex = node_shader_buts_script_ex;
      break;
    case SH_NODE_UVMAP:
      ntype->draw_buttons = node_shader_buts_uvmap;
      break;
    case SH_NODE_VERTEX_COLOR:
      ntype->draw_buttons = node_shader_buts_vertex_color;
      break;
    case SH_NODE_UVALONGSTROKE:
      ntype->draw_buttons = node_shader_buts_uvalongstroke;
      break;
    case SH_NODE_OUTPUT_MATERIAL:
    case SH_NODE_OUTPUT_LIGHT:
    case SH_NODE_OUTPUT_WORLD:
      ntype->draw_buttons = node_buts_output_shader;
      break;
    case SH_NODE_OUTPUT_LINESTYLE:
      ntype->draw_buttons = node_buts_output_linestyle;
      break;
    case SH_NODE_TEX_IES:
      ntype->draw_buttons = node_shader_buts_ies;
      break;
    case SH_NODE_BEVEL:
      ntype->draw_buttons = node_shader_buts_bevel;
      break;
    case SH_NODE_AMBIENT_OCCLUSION:
      ntype->draw_buttons = node_shader_buts_ambient_occlusion;
      break;
    case SH_NODE_TEX_WHITE_NOISE:
      ntype->draw_buttons = node_shader_buts_white_noise;
      break;
    case SH_NODE_OUTPUT_AOV:
      ntype->draw_buttons = node_shader_buts_output_aov;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR COMPOSITE NODES ***************** */

static void node_buts_image_views(uiLayout *layout,
                                  bContext *UNUSED(C),
                                  PointerRNA *ptr,
                                  PointerRNA *imaptr)
{
  uiLayout *col;

  if (!imaptr->data) {
    return;
  }

  col = uiLayoutColumn(layout, false);

  if (RNA_boolean_get(ptr, "has_views")) {
    if (RNA_enum_get(ptr, "view") == 0) {
      uiItemR(col, ptr, "view", DEFAULT_FLAGS, NULL, ICON_CAMERA_STEREO);
    }
    else {
      uiItemR(col, ptr, "view", DEFAULT_FLAGS, NULL, ICON_SCENE);
    }
  }
}

static void node_composit_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  PointerRNA imaptr, iuserptr;

  RNA_pointer_create(ptr->owner_id, &RNA_ImageUser, node->storage, &iuserptr);
  uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
  uiTemplateID(layout,
               C,
               ptr,
               "image",
               "IMAGE_OT_new",
               "IMAGE_OT_open",
               NULL,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               NULL);
  if (!node->id) {
    return;
  }

  imaptr = RNA_pointer_get(ptr, "image");

  node_buts_image_user(layout, C, ptr, &imaptr, &iuserptr, true);

  node_buts_image_views(layout, C, ptr, &imaptr);
}

static void node_composit_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  PointerRNA iuserptr;

  RNA_pointer_create(ptr->owner_id, &RNA_ImageUser, node->storage, &iuserptr);
  uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0, 1);
}

static void node_composit_buts_viewlayers(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  uiLayout *col, *row;
  PointerRNA op_ptr;
  PointerRNA scn_ptr;
  PropertyRNA *prop;
  const char *layer_name;
  char scene_name[MAX_ID_NAME - 2];

  uiTemplateID(layout, C, ptr, "scene", NULL, NULL, NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (!node->id) {
    return;
  }

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "layer", DEFAULT_FLAGS, "", ICON_NONE);

  prop = RNA_struct_find_property(ptr, "layer");
  if (!(RNA_property_enum_identifier(
          C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name))) {
    return;
  }

  scn_ptr = RNA_pointer_get(ptr, "scene");
  RNA_string_get(&scn_ptr, "name", scene_name);

  uiItemFullO(
      row, "RENDER_OT_render", "", ICON_RENDER_STILL, NULL, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
  RNA_string_set(&op_ptr, "layer", layer_name);
  RNA_string_set(&op_ptr, "scene", scene_name);
}

static void node_composit_buts_blur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col, *row;
  int reference;
  int filter;

  col = uiLayoutColumn(layout, false);
  filter = RNA_enum_get(ptr, "filter_type");
  reference = RNA_boolean_get(ptr, "use_variable_size");

  uiItemR(col, ptr, "filter_type", DEFAULT_FLAGS, "", ICON_NONE);
  if (filter != R_FILTER_FAST_GAUSS) {
    uiItemR(col, ptr, "use_variable_size", DEFAULT_FLAGS, NULL, ICON_NONE);
    if (!reference) {
      uiItemR(col, ptr, "use_bokeh", DEFAULT_FLAGS, NULL, ICON_NONE);
    }
    uiItemR(col, ptr, "use_gamma_correction", DEFAULT_FLAGS, NULL, ICON_NONE);
  }

  uiItemR(col, ptr, "use_relative", DEFAULT_FLAGS, NULL, ICON_NONE);

  if (RNA_boolean_get(ptr, "use_relative")) {
    uiItemL(col, IFACE_("Aspect Correction"), ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row, ptr, "aspect_correction", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "factor_x", DEFAULT_FLAGS, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "factor_y", DEFAULT_FLAGS, IFACE_("Y"), ICON_NONE);
  }
  else {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "size_x", DEFAULT_FLAGS, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "size_y", DEFAULT_FLAGS, IFACE_("Y"), ICON_NONE);
  }
  uiItemR(col, ptr, "use_extended_bounds", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_dblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  uiItemR(layout, ptr, "iterations", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_wrap", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemL(col, IFACE_("Center:"), ICON_NONE);
  uiItemR(col, ptr, "center_x", DEFAULT_FLAGS, IFACE_("X"), ICON_NONE);
  uiItemR(col, ptr, "center_y", DEFAULT_FLAGS, IFACE_("Y"), ICON_NONE);

  uiItemS(layout);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "distance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "angle", DEFAULT_FLAGS, NULL, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, ptr, "spin", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "zoom", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_bilateralblur(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "iterations", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "sigma_color", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "sigma_space", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_defocus(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = uiLayoutColumn(layout, false);
  uiItemL(col, IFACE_("Bokeh Type:"), ICON_NONE);
  uiItemR(col, ptr, "bokeh", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(col, ptr, "angle", DEFAULT_FLAGS, NULL, ICON_NONE);

  uiItemR(layout, ptr, "use_gamma_correction", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_zbuffer") == true);
  uiItemR(col, ptr, "f_stop", DEFAULT_FLAGS, NULL, ICON_NONE);

  uiItemR(layout, ptr, "blur_max", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "threshold", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_preview", DEFAULT_FLAGS, NULL, ICON_NONE);

  uiTemplateID(layout, C, ptr, "scene", NULL, NULL, NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_zbuffer", DEFAULT_FLAGS, NULL, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_zbuffer") == false);
  uiItemR(sub, ptr, "z_scale", DEFAULT_FLAGS, NULL, ICON_NONE);
}

/* qdn: glare node */
static void node_composit_buts_glare(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "glare_type", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "quality", DEFAULT_FLAGS, "", ICON_NONE);

  if (RNA_enum_get(ptr, "glare_type") != 1) {
    uiItemR(layout, ptr, "iterations", DEFAULT_FLAGS, NULL, ICON_NONE);

    if (RNA_enum_get(ptr, "glare_type") != 0) {
      uiItemR(layout, ptr, "color_modulation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
    }
  }

  uiItemR(layout, ptr, "mix", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "threshold", DEFAULT_FLAGS, NULL, ICON_NONE);

  if (RNA_enum_get(ptr, "glare_type") == 2) {
    uiItemR(layout, ptr, "streaks", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(layout, ptr, "angle_offset", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  if (RNA_enum_get(ptr, "glare_type") == 0 || RNA_enum_get(ptr, "glare_type") == 2) {
    uiItemR(layout, ptr, "fade", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

    if (RNA_enum_get(ptr, "glare_type") == 0) {
      uiItemR(layout, ptr, "use_rotate_45", DEFAULT_FLAGS, NULL, ICON_NONE);
    }
  }
  if (RNA_enum_get(ptr, "glare_type") == 1) {
    uiItemR(layout, ptr, "size", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_composit_buts_tonemap(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "tonemap_type", DEFAULT_FLAGS, "", ICON_NONE);
  if (RNA_enum_get(ptr, "tonemap_type") == 0) {
    uiItemR(col, ptr, "key", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
    uiItemR(col, ptr, "offset", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "gamma", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "intensity", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
    uiItemR(col, ptr, "adaptation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
    uiItemR(col, ptr, "correction", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  }
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_projector", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(col, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector") == false);
  uiItemR(col, ptr, "use_jitter", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_fit", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_vecblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "samples", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "factor", DEFAULT_FLAGS, IFACE_("Blur"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemL(col, IFACE_("Speed:"), ICON_NONE);
  uiItemR(col, ptr, "speed_min", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
  uiItemR(col, ptr, "speed_max", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);

  uiItemR(layout, ptr, "use_curved", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_filter(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_flip(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "axis", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_crop(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  uiItemR(layout, ptr, "use_crop_size", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "relative", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  if (RNA_boolean_get(ptr, "relative")) {
    uiItemR(col, ptr, "rel_min_x", DEFAULT_FLAGS, IFACE_("Left"), ICON_NONE);
    uiItemR(col, ptr, "rel_max_x", DEFAULT_FLAGS, IFACE_("Right"), ICON_NONE);
    uiItemR(col, ptr, "rel_min_y", DEFAULT_FLAGS, IFACE_("Up"), ICON_NONE);
    uiItemR(col, ptr, "rel_max_y", DEFAULT_FLAGS, IFACE_("Down"), ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "min_x", DEFAULT_FLAGS, IFACE_("Left"), ICON_NONE);
    uiItemR(col, ptr, "max_x", DEFAULT_FLAGS, IFACE_("Right"), ICON_NONE);
    uiItemR(col, ptr, "min_y", DEFAULT_FLAGS, IFACE_("Up"), ICON_NONE);
    uiItemR(col, ptr, "max_y", DEFAULT_FLAGS, IFACE_("Down"), ICON_NONE);
  }
}

static void node_composit_buts_splitviewer(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row, *col;

  col = uiLayoutColumn(layout, false);
  row = uiLayoutRow(col, false);
  uiItemR(row, ptr, "axis", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  uiItemR(col, ptr, "factor", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_double_edge_mask(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemL(col, IFACE_("Inner Edge:"), ICON_NONE);
  uiItemR(col, ptr, "inner_mode", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemL(col, IFACE_("Buffer Edge:"), ICON_NONE);
  uiItemR(col, ptr, "edge_mode", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_map_range(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_clamp", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_map_value(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "offset", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "size", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_min", DEFAULT_FLAGS, NULL, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
  uiItemR(sub, ptr, "min", DEFAULT_FLAGS, "", ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_max", DEFAULT_FLAGS, NULL, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
  uiItemR(sub, ptr, "max", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_premultiply", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "premul", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_zcombine(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_alpha", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_antialias_z", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "distance", DEFAULT_FLAGS, NULL, ICON_NONE);
  switch (RNA_enum_get(ptr, "mode")) {
    case CMP_NODE_DILATEERODE_DISTANCE_THRESH:
      uiItemR(layout, ptr, "edge", DEFAULT_FLAGS, NULL, ICON_NONE);
      break;
    case CMP_NODE_DILATEERODE_DISTANCE_FEATHER:
      uiItemR(layout, ptr, "falloff", DEFAULT_FLAGS, NULL, ICON_NONE);
      break;
  }
}

static void node_composit_buts_inpaint(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distance", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_despeckle(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "threshold", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "threshold_neighbor", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_diff_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "tolerance", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "falloff", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_distance_matte(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiLayout *col, *row;

  col = uiLayoutColumn(layout, true);

  uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "channel", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiItemR(col, ptr, "tolerance", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "falloff", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_color_spill(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row, *col;

  uiItemL(layout, IFACE_("Despill Channel:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "channel", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "limit_method", DEFAULT_FLAGS, NULL, ICON_NONE);

  if (RNA_enum_get(ptr, "limit_method") == 0) {
    uiItemL(col, IFACE_("Limiting Channel:"), ICON_NONE);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "limit_channel", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  }

  uiItemR(col, ptr, "ratio", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "use_unspill", DEFAULT_FLAGS, NULL, ICON_NONE);
  if (RNA_boolean_get(ptr, "use_unspill") == true) {
    uiItemR(col, ptr, "unspill_red", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
    uiItemR(col, ptr, "unspill_green", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
    uiItemR(col, ptr, "unspill_blue", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  }
}

static void node_composit_buts_chroma_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "tolerance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "threshold", DEFAULT_FLAGS, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  /*uiItemR(col, ptr, "lift", UI_ITEM_R_SLIDER, NULL, ICON_NONE);  Removed for now */
  uiItemR(col, ptr, "gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  /*uiItemR(col, ptr, "shadow_adjust", UI_ITEM_R_SLIDER, NULL, ICON_NONE);  Removed for now*/
}

static void node_composit_buts_color_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "color_hue", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "color_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "color_value", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_channel_matte(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiLayout *col, *row;

  uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "color_space", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemL(col, IFACE_("Key Channel:"), ICON_NONE);
  row = uiLayoutRow(col, false);
  uiItemR(row, ptr, "matte_channel", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "limit_method", DEFAULT_FLAGS, NULL, ICON_NONE);
  if (RNA_enum_get(ptr, "limit_method") == 0) {
    uiItemL(col, IFACE_("Limiting Channel:"), ICON_NONE);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "limit_channel", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  }

  uiItemR(col, ptr, "limit_max", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "limit_min", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_luma_matte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "limit_max", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(col, ptr, "limit_min", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "alpha", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_id_mask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "index", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_antialiasing", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_file_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  PointerRNA imfptr = RNA_pointer_get(ptr, "format");
  const bool multilayer = RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_MULTILAYER;

  if (multilayer) {
    uiItemL(layout, IFACE_("Path:"), ICON_NONE);
  }
  else {
    uiItemL(layout, IFACE_("Base Path:"), ICON_NONE);
  }
  uiItemR(layout, ptr, "base_path", DEFAULT_FLAGS, "", ICON_NONE);
}
static void node_composit_buts_file_output_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  PointerRNA imfptr = RNA_pointer_get(ptr, "format");
  PointerRNA active_input_ptr, op_ptr;
  wmOperatorType *ot;
  uiLayout *row, *col;
  int active_index;
  const bool multilayer = RNA_enum_get(&imfptr, "file_format") == R_IMF_IMTYPE_MULTILAYER;
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;

  node_composit_buts_file_output(layout, C, ptr);
  uiTemplateImageSettings(layout, &imfptr, false);

  /* disable stereo output for multilayer, too much work for something that no one will use */
  /* if someone asks for that we can implement it */
  if (is_multiview) {
    uiTemplateImageFormatViews(layout, &imfptr, NULL);
  }

  uiItemS(layout);

  uiItemO(layout, IFACE_("Add Input"), ICON_ADD, "NODE_OT_output_file_add_socket");

  row = uiLayoutRow(layout, false);
  col = uiLayoutColumn(row, true);

  active_index = RNA_int_get(ptr, "active_input_index");
  /* using different collection properties if multilayer format is enabled */
  if (multilayer) {
    uiTemplateList(col,
                   C,
                   "UI_UL_list",
                   "file_output_node",
                   ptr,
                   "layer_slots",
                   ptr,
                   "active_input_index",
                   NULL,
                   0,
                   0,
                   0,
                   0,
                   false,
                   false);
    RNA_property_collection_lookup_int(
        ptr, RNA_struct_find_property(ptr, "layer_slots"), active_index, &active_input_ptr);
  }
  else {
    uiTemplateList(col,
                   C,
                   "UI_UL_list",
                   "file_output_node",
                   ptr,
                   "file_slots",
                   ptr,
                   "active_input_index",
                   NULL,
                   0,
                   0,
                   0,
                   0,
                   false,
                   false);
    RNA_property_collection_lookup_int(
        ptr, RNA_struct_find_property(ptr, "file_slots"), active_index, &active_input_ptr);
  }
  /* XXX collection lookup does not return the ID part of the pointer,
   * setting this manually here */
  active_input_ptr.owner_id = ptr->owner_id;

  col = uiLayoutColumn(row, true);
  ot = WM_operatortype_find("NODE_OT_output_file_move_active_socket", false);
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_UP, NULL, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
  RNA_enum_set(&op_ptr, "direction", 1);
  uiItemFullO_ptr(col, ot, "", ICON_TRIA_DOWN, NULL, WM_OP_INVOKE_DEFAULT, 0, &op_ptr);
  RNA_enum_set(&op_ptr, "direction", 2);

  if (active_input_ptr.data) {
    if (multilayer) {
      col = uiLayoutColumn(layout, true);

      uiItemL(col, IFACE_("Layer:"), ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &active_input_ptr, "name", DEFAULT_FLAGS, "", ICON_NONE);
      uiItemFullO(row,
                  "NODE_OT_output_file_remove_active_socket",
                  "",
                  ICON_X,
                  NULL,
                  WM_OP_EXEC_DEFAULT,
                  UI_ITEM_R_ICON_ONLY,
                  NULL);
    }
    else {
      col = uiLayoutColumn(layout, true);

      uiItemL(col, IFACE_("File Subpath:"), ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &active_input_ptr, "path", DEFAULT_FLAGS, "", ICON_NONE);
      uiItemFullO(row,
                  "NODE_OT_output_file_remove_active_socket",
                  "",
                  ICON_X,
                  NULL,
                  WM_OP_EXEC_DEFAULT,
                  UI_ITEM_R_ICON_ONLY,
                  NULL);

      /* format details for individual files */
      imfptr = RNA_pointer_get(&active_input_ptr, "format");

      col = uiLayoutColumn(layout, true);
      uiItemL(col, IFACE_("Format:"), ICON_NONE);
      uiItemR(col, &active_input_ptr, "use_node_format", DEFAULT_FLAGS, NULL, ICON_NONE);

      col = uiLayoutColumn(layout, false);
      uiLayoutSetActive(col, RNA_boolean_get(&active_input_ptr, "use_node_format") == false);
      uiTemplateImageSettings(col, &imfptr, false);

      if (is_multiview) {
        uiTemplateImageFormatViews(layout, &imfptr, NULL);
      }
    }
  }
}

static void node_composit_buts_scale(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", DEFAULT_FLAGS, "", ICON_NONE);

  if (RNA_enum_get(ptr, "space") == CMP_SCALE_RENDERPERCENT) {
    uiLayout *row;
    uiItemR(layout, ptr, "frame_method", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row, ptr, "offset_x", DEFAULT_FLAGS, "X", ICON_NONE);
    uiItemR(row, ptr, "offset_y", DEFAULT_FLAGS, "Y", ICON_NONE);
  }
}

static void node_composit_buts_rotate(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_invert(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "invert_rgb", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(col, ptr, "invert_alpha", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_premulkey(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mapping", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_view_levels(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "channel", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
}

static void node_composit_buts_colorbalance(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *split, *col, *row;

  uiItemR(layout, ptr, "correction_method", DEFAULT_FLAGS, NULL, ICON_NONE);

  if (RNA_enum_get(ptr, "correction_method") == 0) {

    split = uiLayoutSplit(layout, 0.0f, false);
    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "lift", 1, 1, 0, 1);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "lift", DEFAULT_FLAGS, NULL, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "gamma", 1, 1, 1, 1);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "gamma", DEFAULT_FLAGS, NULL, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "gain", 1, 1, 1, 1);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "gain", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  else {

    split = uiLayoutSplit(layout, 0.0f, false);
    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "offset", 1, 1, 0, 1);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "offset", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "offset_basis", DEFAULT_FLAGS, NULL, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "power", 1, 1, 0, 1);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "power", DEFAULT_FLAGS, NULL, ICON_NONE);

    col = uiLayoutColumn(split, false);
    uiTemplateColorPicker(col, ptr, "slope", 1, 1, 0, 1);
    row = uiLayoutRow(col, false);
    uiItemR(row, ptr, "slope", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}
static void node_composit_buts_colorbalance_ex(uiLayout *layout,
                                               bContext *UNUSED(C),
                                               PointerRNA *ptr)
{
  uiItemR(layout, ptr, "correction_method", DEFAULT_FLAGS, NULL, ICON_NONE);

  if (RNA_enum_get(ptr, "correction_method") == 0) {

    uiTemplateColorPicker(layout, ptr, "lift", 1, 1, 0, 1);
    uiItemR(layout, ptr, "lift", DEFAULT_FLAGS, NULL, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "gamma", 1, 1, 1, 1);
    uiItemR(layout, ptr, "gamma", DEFAULT_FLAGS, NULL, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "gain", 1, 1, 1, 1);
    uiItemR(layout, ptr, "gain", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
  else {
    uiTemplateColorPicker(layout, ptr, "offset", 1, 1, 0, 1);
    uiItemR(layout, ptr, "offset", DEFAULT_FLAGS, NULL, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "power", 1, 1, 0, 1);
    uiItemR(layout, ptr, "power", DEFAULT_FLAGS, NULL, ICON_NONE);

    uiTemplateColorPicker(layout, ptr, "slope", 1, 1, 0, 1);
    uiItemR(layout, ptr, "slope", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_composit_buts_huecorrect(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = ptr->data;
  CurveMapping *cumap = node->storage;

  if (_sample_col[0] != SAMPLE_FLT_ISNONE) {
    cumap->flag |= CUMA_DRAW_SAMPLE;
    copy_v3_v3(cumap->sample, _sample_col);
  }
  else {
    cumap->flag &= ~CUMA_DRAW_SAMPLE;
  }

  uiTemplateCurveMapping(layout, ptr, "mapping", 'h', false, false, false, false);
}

static void node_composit_buts_ycc(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_movieclip(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(
      layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);
}

static void node_composit_buts_movieclip_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  PointerRNA clipptr;

  uiTemplateID(
      layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (!node->id) {
    return;
  }

  clipptr = RNA_pointer_get(ptr, "clip");

  uiTemplateColorspaceSettings(layout, &clipptr, "colorspace_settings");
}

static void node_composit_buts_stabilize2d(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;

  uiTemplateID(
      layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (!node->id) {
    return;
  }

  uiItemR(layout, ptr, "filter_type", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "invert", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_translate(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_relative", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "wrap_axis", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_transform(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_moviedistortion(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;

  uiTemplateID(
      layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (!node->id) {
    return;
  }

  uiItemR(layout, ptr, "distortion_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_colorcorrection(uiLayout *layout,
                                               bContext *UNUSED(C),
                                               PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "red", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "green", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "blue", DEFAULT_FLAGS, NULL, ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, "", ICON_NONE);
  uiItemL(row, IFACE_("Saturation"), ICON_NONE);
  uiItemL(row, IFACE_("Contrast"), ICON_NONE);
  uiItemL(row, IFACE_("Gamma"), ICON_NONE);
  uiItemL(row, IFACE_("Gain"), ICON_NONE);
  uiItemL(row, IFACE_("Lift"), ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Master"), ICON_NONE);
  uiItemR(row, ptr, "master_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "master_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Highlights"), ICON_NONE);
  uiItemR(row, ptr, "highlights_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "highlights_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "highlights_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "highlights_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "highlights_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Midtones"), ICON_NONE);
  uiItemR(row, ptr, "midtones_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "midtones_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "midtones_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "midtones_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "midtones_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemL(row, IFACE_("Shadows"), ICON_NONE);
  uiItemR(row, ptr, "shadows_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemR(row, ptr, "shadows_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "midtones_start", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_end", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_colorcorrection_ex(uiLayout *layout,
                                                  bContext *UNUSED(C),
                                                  PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "red", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "green", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "blue", DEFAULT_FLAGS, NULL, ICON_NONE);
  row = layout;
  uiItemL(row, IFACE_("Saturation"), ICON_NONE);
  uiItemR(row, ptr, "master_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "highlights_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "shadows_saturation", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemL(row, IFACE_("Contrast"), ICON_NONE);
  uiItemR(row, ptr, "master_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "highlights_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "shadows_contrast", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemL(row, IFACE_("Gamma"), ICON_NONE);
  uiItemR(row, ptr, "master_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "highlights_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "shadows_gamma", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemL(row, IFACE_("Gain"), ICON_NONE);
  uiItemR(row, ptr, "master_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "highlights_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "shadows_gain", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemL(row, IFACE_("Lift"), ICON_NONE);
  uiItemR(row, ptr, "master_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "highlights_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "shadows_lift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "midtones_start", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "midtones_end", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_switch(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "check", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_switch_view_ex(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *UNUSED(ptr))
{
  uiItemFullO(layout,
              "NODE_OT_switch_view_update",
              "Update Views",
              ICON_FILE_REFRESH,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              NULL);
}

static void node_composit_buts_boxmask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row;

  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "x", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "y", DEFAULT_FLAGS, NULL, ICON_NONE);

  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "width", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "height", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemR(layout, ptr, "rotation", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "mask_type", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_bokehimage(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "flaps", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "angle", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "rounding", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(layout, ptr, "catadioptric", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(layout, ptr, "shift", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_bokehblur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_variable_size", DEFAULT_FLAGS, NULL, ICON_NONE);
  // uiItemR(layout, ptr, "f_stop", DEFAULT_FLAGS, NULL, ICON_NONE);  // UNUSED
  uiItemR(layout, ptr, "blur_max", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_extended_bounds", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_backdrop_viewer(
    SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y)
{
  //  node_composit_backdrop_canvas(snode, backdrop, node, x, y);
  if (node->custom1 == 0) {
    const float backdropWidth = backdrop->x;
    const float backdropHeight = backdrop->y;
    const float cx = x + snode->zoom * backdropWidth * node->custom3;
    const float cy = y + snode->zoom * backdropHeight * node->custom4;
    const float cross_size = 12 * U.pixelsize;

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    immUniformColor3f(1.0f, 1.0f, 1.0f);

    immBegin(GPU_PRIM_LINES, 4);
    immVertex2f(pos, cx - cross_size, cy - cross_size);
    immVertex2f(pos, cx + cross_size, cy + cross_size);
    immVertex2f(pos, cx + cross_size, cy - cross_size);
    immVertex2f(pos, cx - cross_size, cy + cross_size);
    immEnd();

    immUnbindProgram();
  }
}

static void node_composit_backdrop_boxmask(
    SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y)
{
  NodeBoxMask *boxmask = node->storage;
  const float backdropWidth = backdrop->x;
  const float backdropHeight = backdrop->y;
  const float aspect = backdropWidth / backdropHeight;
  const float rad = -boxmask->rotation;
  const float cosine = cosf(rad);
  const float sine = sinf(rad);
  const float halveBoxWidth = backdropWidth * (boxmask->width / 2.0f);
  const float halveBoxHeight = backdropHeight * (boxmask->height / 2.0f) * aspect;

  float cx, cy, x1, x2, x3, x4;
  float y1, y2, y3, y4;

  cx = x + snode->zoom * backdropWidth * boxmask->x;
  cy = y + snode->zoom * backdropHeight * boxmask->y;

  x1 = cx - (cosine * halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
  x2 = cx - (cosine * -halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
  x3 = cx - (cosine * -halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
  x4 = cx - (cosine * halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
  y1 = cy - (-sine * halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
  y2 = cy - (-sine * -halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
  y3 = cy - (-sine * -halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;
  y4 = cy - (-sine * halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor3f(1.0f, 1.0f, 1.0f);

  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x3, y3);
  immVertex2f(pos, x4, y4);
  immEnd();

  immUnbindProgram();
}

static void node_composit_backdrop_ellipsemask(
    SpaceNode *snode, ImBuf *backdrop, bNode *node, int x, int y)
{
  NodeEllipseMask *ellipsemask = node->storage;
  const float backdropWidth = backdrop->x;
  const float backdropHeight = backdrop->y;
  const float aspect = backdropWidth / backdropHeight;
  const float rad = -ellipsemask->rotation;
  const float cosine = cosf(rad);
  const float sine = sinf(rad);
  const float halveBoxWidth = backdropWidth * (ellipsemask->width / 2.0f);
  const float halveBoxHeight = backdropHeight * (ellipsemask->height / 2.0f) * aspect;

  float cx, cy, x1, x2, x3, x4;
  float y1, y2, y3, y4;

  cx = x + snode->zoom * backdropWidth * ellipsemask->x;
  cy = y + snode->zoom * backdropHeight * ellipsemask->y;

  x1 = cx - (cosine * halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
  x2 = cx - (cosine * -halveBoxWidth + sine * halveBoxHeight) * snode->zoom;
  x3 = cx - (cosine * -halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
  x4 = cx - (cosine * halveBoxWidth + sine * -halveBoxHeight) * snode->zoom;
  y1 = cy - (-sine * halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
  y2 = cy - (-sine * -halveBoxWidth + cosine * halveBoxHeight) * snode->zoom;
  y3 = cy - (-sine * -halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;
  y4 = cy - (-sine * halveBoxWidth + cosine * -halveBoxHeight) * snode->zoom;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor3f(1.0f, 1.0f, 1.0f);

  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x3, y3);
  immVertex2f(pos, x4, y4);
  immEnd();

  immUnbindProgram();
}

static void node_composit_buts_ellipsemask(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *row;
  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "x", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(row, ptr, "y", DEFAULT_FLAGS, NULL, ICON_NONE);
  row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "width", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
  uiItemR(row, ptr, "height", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  uiItemR(layout, ptr, "rotation", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "mask_type", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_composite(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_alpha", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_viewer(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_alpha", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_viewer_ex(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  uiItemR(layout, ptr, "use_alpha", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "tile_order", DEFAULT_FLAGS, NULL, ICON_NONE);
  if (RNA_enum_get(ptr, "tile_order") == 0) {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "center_x", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(col, ptr, "center_y", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_composit_buts_mask(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;

  uiTemplateID(layout, C, ptr, "mask", NULL, NULL, NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);
  uiItemR(layout, ptr, "use_feather", DEFAULT_FLAGS, NULL, ICON_NONE);

  uiItemR(layout, ptr, "size_source", DEFAULT_FLAGS, "", ICON_NONE);

  if (node->custom1 & (CMP_NODEFLAG_MASK_FIXED | CMP_NODEFLAG_MASK_FIXED_SCENE)) {
    uiItemR(layout, ptr, "size_x", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(layout, ptr, "size_y", DEFAULT_FLAGS, NULL, ICON_NONE);
  }

  uiItemR(layout, ptr, "use_motion_blur", DEFAULT_FLAGS, NULL, ICON_NONE);
  if (node->custom1 & CMP_NODEFLAG_MASK_MOTION_BLUR) {
    uiItemR(layout, ptr, "motion_blur_samples", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(layout, ptr, "motion_blur_shutter", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_composit_buts_keyingscreen(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;

  uiTemplateID(layout, C, ptr, "clip", NULL, NULL, NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    uiLayout *col;
    PointerRNA tracking_ptr;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, &clip->tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, true);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);
  }
}

static void node_composit_buts_keying(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  /* bNode *node = ptr->data; */ /* UNUSED */

  uiItemR(layout, ptr, "blur_pre", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "screen_balance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "despill_factor", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "despill_balance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "edge_kernel_radius", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "edge_kernel_tolerance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "clip_black", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "clip_white", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "dilate_distance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "feather_falloff", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "feather_distance", DEFAULT_FLAGS, NULL, ICON_NONE);
  uiItemR(layout, ptr, "blur_post", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_trackpos(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;

  uiTemplateID(
      layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *object;
    uiLayout *col;
    PointerRNA tracking_ptr;
    NodeTrackPosData *data = node->storage;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, false);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

    object = BKE_tracking_object_get_named(tracking, data->tracking_object);
    if (object) {
      PointerRNA object_ptr;

      RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, object, &object_ptr);

      uiItemPointerR(col, ptr, "track_name", &object_ptr, "tracks", "", ICON_ANIM_DATA);
    }
    else {
      uiItemR(layout, ptr, "track_name", DEFAULT_FLAGS, "", ICON_ANIM_DATA);
    }

    uiItemR(layout, ptr, "position", DEFAULT_FLAGS, NULL, ICON_NONE);

    if (ELEM(node->custom1, CMP_TRACKPOS_RELATIVE_FRAME, CMP_TRACKPOS_ABSOLUTE_FRAME)) {
      uiItemR(layout, ptr, "frame_relative", DEFAULT_FLAGS, NULL, ICON_NONE);
    }
  }
}

static void node_composit_buts_planetrackdeform(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  NodePlaneTrackDeformData *data = node->storage;

  uiTemplateID(
      layout, C, ptr, "clip", NULL, "CLIP_OT_open", NULL, UI_TEMPLATE_ID_FILTER_ALL, false, NULL);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *object;
    uiLayout *col;
    PointerRNA tracking_ptr;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, false);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

    object = BKE_tracking_object_get_named(tracking, data->tracking_object);
    if (object) {
      PointerRNA object_ptr;

      RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, object, &object_ptr);

      uiItemPointerR(
          col, ptr, "plane_track_name", &object_ptr, "plane_tracks", "", ICON_ANIM_DATA);
    }
    else {
      uiItemR(layout, ptr, "plane_track_name", 0, "", ICON_ANIM_DATA);
    }
  }

  uiItemR(layout, ptr, "use_motion_blur", DEFAULT_FLAGS, NULL, ICON_NONE);
  if (data->flag & CMP_NODEFLAG_PLANETRACKDEFORM_MOTION_BLUR) {
    uiItemR(layout, ptr, "motion_blur_samples", DEFAULT_FLAGS, NULL, ICON_NONE);
    uiItemR(layout, ptr, "motion_blur_shutter", DEFAULT_FLAGS, NULL, ICON_NONE);
  }
}

static void node_composit_buts_cornerpin(uiLayout *UNUSED(layout),
                                         bContext *UNUSED(C),
                                         PointerRNA *UNUSED(ptr))
{
}

static void node_composit_buts_sunbeams(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "source", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, "", ICON_NONE);
  uiItemR(layout, ptr, "ray_length", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, NULL, ICON_NONE);
}

static void node_composit_buts_cryptomatte(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);

  uiItemL(col, IFACE_("Matte Objects:"), ICON_NONE);

  uiLayout *row = uiLayoutRow(col, true);
  uiTemplateCryptoPicker(row, ptr, "add");
  uiTemplateCryptoPicker(row, ptr, "remove");

  uiItemR(col, ptr, "matte_id", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_cryptomatte_ex(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *UNUSED(ptr))
{
  uiItemO(layout, IFACE_("Add Crypto Layer"), ICON_ADD, "NODE_OT_cryptomatte_layer_add");
  uiItemO(layout, IFACE_("Remove Crypto Layer"), ICON_REMOVE, "NODE_OT_cryptomatte_layer_remove");
}

static void node_composit_buts_brightcontrast(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_premultiply", DEFAULT_FLAGS, NULL, ICON_NONE);
}

static void node_composit_buts_denoise(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
#ifndef WITH_OPENIMAGEDENOISE
  uiItemL(layout, IFACE_("Disabled, built without OpenImageDenoise"), ICON_ERROR);
#else
  if (!BLI_cpu_support_sse41()) {
    uiItemL(layout, IFACE_("Disabled, CPU with SSE4.1 is required"), ICON_ERROR);
  }
#endif

  uiItemR(layout, ptr, "use_hdr", DEFAULT_FLAGS, NULL, ICON_NONE);
}

/* only once called */
static void node_composit_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case CMP_NODE_IMAGE:
      ntype->draw_buttons = node_composit_buts_image;
      ntype->draw_buttons_ex = node_composit_buts_image_ex;
      break;
    case CMP_NODE_R_LAYERS:
      ntype->draw_buttons = node_composit_buts_viewlayers;
      break;
    case CMP_NODE_NORMAL:
      ntype->draw_buttons = node_buts_normal;
      break;
    case CMP_NODE_CURVE_VEC:
      ntype->draw_buttons = node_buts_curvevec;
      break;
    case CMP_NODE_CURVE_RGB:
      ntype->draw_buttons = node_buts_curvecol;
      break;
    case CMP_NODE_VALUE:
      ntype->draw_buttons = node_buts_value;
      break;
    case CMP_NODE_RGB:
      ntype->draw_buttons = node_buts_rgb;
      break;
    case CMP_NODE_FLIP:
      ntype->draw_buttons = node_composit_buts_flip;
      break;
    case CMP_NODE_SPLITVIEWER:
      ntype->draw_buttons = node_composit_buts_splitviewer;
      break;
    case CMP_NODE_MIX_RGB:
      ntype->draw_buttons = node_buts_mix_rgb;
      break;
    case CMP_NODE_VALTORGB:
      ntype->draw_buttons = node_buts_colorramp;
      break;
    case CMP_NODE_CROP:
      ntype->draw_buttons = node_composit_buts_crop;
      break;
    case CMP_NODE_BLUR:
      ntype->draw_buttons = node_composit_buts_blur;
      break;
    case CMP_NODE_DBLUR:
      ntype->draw_buttons = node_composit_buts_dblur;
      break;
    case CMP_NODE_BILATERALBLUR:
      ntype->draw_buttons = node_composit_buts_bilateralblur;
      break;
    case CMP_NODE_DEFOCUS:
      ntype->draw_buttons = node_composit_buts_defocus;
      break;
    case CMP_NODE_GLARE:
      ntype->draw_buttons = node_composit_buts_glare;
      break;
    case CMP_NODE_TONEMAP:
      ntype->draw_buttons = node_composit_buts_tonemap;
      break;
    case CMP_NODE_LENSDIST:
      ntype->draw_buttons = node_composit_buts_lensdist;
      break;
    case CMP_NODE_VECBLUR:
      ntype->draw_buttons = node_composit_buts_vecblur;
      break;
    case CMP_NODE_FILTER:
      ntype->draw_buttons = node_composit_buts_filter;
      break;
    case CMP_NODE_MAP_VALUE:
      ntype->draw_buttons = node_composit_buts_map_value;
      break;
    case CMP_NODE_MAP_RANGE:
      ntype->draw_buttons = node_composit_buts_map_range;
      break;
    case CMP_NODE_TIME:
      ntype->draw_buttons = node_buts_time;
      break;
    case CMP_NODE_ALPHAOVER:
      ntype->draw_buttons = node_composit_buts_alphaover;
      break;
    case CMP_NODE_TEXTURE:
      ntype->draw_buttons = node_buts_texture;
      break;
    case CMP_NODE_DILATEERODE:
      ntype->draw_buttons = node_composit_buts_dilateerode;
      break;
    case CMP_NODE_INPAINT:
      ntype->draw_buttons = node_composit_buts_inpaint;
      break;
    case CMP_NODE_DESPECKLE:
      ntype->draw_buttons = node_composit_buts_despeckle;
      break;
    case CMP_NODE_OUTPUT_FILE:
      ntype->draw_buttons = node_composit_buts_file_output;
      ntype->draw_buttons_ex = node_composit_buts_file_output_ex;
      break;
    case CMP_NODE_DIFF_MATTE:
      ntype->draw_buttons = node_composit_buts_diff_matte;
      break;
    case CMP_NODE_DIST_MATTE:
      ntype->draw_buttons = node_composit_buts_distance_matte;
      break;
    case CMP_NODE_COLOR_SPILL:
      ntype->draw_buttons = node_composit_buts_color_spill;
      break;
    case CMP_NODE_CHROMA_MATTE:
      ntype->draw_buttons = node_composit_buts_chroma_matte;
      break;
    case CMP_NODE_COLOR_MATTE:
      ntype->draw_buttons = node_composit_buts_color_matte;
      break;
    case CMP_NODE_SCALE:
      ntype->draw_buttons = node_composit_buts_scale;
      break;
    case CMP_NODE_ROTATE:
      ntype->draw_buttons = node_composit_buts_rotate;
      break;
    case CMP_NODE_CHANNEL_MATTE:
      ntype->draw_buttons = node_composit_buts_channel_matte;
      break;
    case CMP_NODE_LUMA_MATTE:
      ntype->draw_buttons = node_composit_buts_luma_matte;
      break;
    case CMP_NODE_MAP_UV:
      ntype->draw_buttons = node_composit_buts_map_uv;
      break;
    case CMP_NODE_ID_MASK:
      ntype->draw_buttons = node_composit_buts_id_mask;
      break;
    case CMP_NODE_DOUBLEEDGEMASK:
      ntype->draw_buttons = node_composit_buts_double_edge_mask;
      break;
    case CMP_NODE_MATH:
      ntype->draw_buttons = node_buts_math;
      break;
    case CMP_NODE_INVERT:
      ntype->draw_buttons = node_composit_buts_invert;
      break;
    case CMP_NODE_PREMULKEY:
      ntype->draw_buttons = node_composit_buts_premulkey;
      break;
    case CMP_NODE_VIEW_LEVELS:
      ntype->draw_buttons = node_composit_buts_view_levels;
      break;
    case CMP_NODE_COLORBALANCE:
      ntype->draw_buttons = node_composit_buts_colorbalance;
      ntype->draw_buttons_ex = node_composit_buts_colorbalance_ex;
      break;
    case CMP_NODE_HUECORRECT:
      ntype->draw_buttons = node_composit_buts_huecorrect;
      break;
    case CMP_NODE_ZCOMBINE:
      ntype->draw_buttons = node_composit_buts_zcombine;
      break;
    case CMP_NODE_COMBYCCA:
    case CMP_NODE_SEPYCCA:
      ntype->draw_buttons = node_composit_buts_ycc;
      break;
    case CMP_NODE_MOVIECLIP:
      ntype->draw_buttons = node_composit_buts_movieclip;
      ntype->draw_buttons_ex = node_composit_buts_movieclip_ex;
      break;
    case CMP_NODE_STABILIZE2D:
      ntype->draw_buttons = node_composit_buts_stabilize2d;
      break;
    case CMP_NODE_TRANSFORM:
      ntype->draw_buttons = node_composit_buts_transform;
      break;
    case CMP_NODE_TRANSLATE:
      ntype->draw_buttons = node_composit_buts_translate;
      break;
    case CMP_NODE_MOVIEDISTORTION:
      ntype->draw_buttons = node_composit_buts_moviedistortion;
      break;
    case CMP_NODE_COLORCORRECTION:
      ntype->draw_buttons = node_composit_buts_colorcorrection;
      ntype->draw_buttons_ex = node_composit_buts_colorcorrection_ex;
      break;
    case CMP_NODE_SWITCH:
      ntype->draw_buttons = node_composit_buts_switch;
      break;
    case CMP_NODE_SWITCH_VIEW:
      ntype->draw_buttons_ex = node_composit_buts_switch_view_ex;
      break;
    case CMP_NODE_MASK_BOX:
      ntype->draw_buttons = node_composit_buts_boxmask;
      ntype->draw_backdrop = node_composit_backdrop_boxmask;
      break;
    case CMP_NODE_MASK_ELLIPSE:
      ntype->draw_buttons = node_composit_buts_ellipsemask;
      ntype->draw_backdrop = node_composit_backdrop_ellipsemask;
      break;
    case CMP_NODE_BOKEHIMAGE:
      ntype->draw_buttons = node_composit_buts_bokehimage;
      break;
    case CMP_NODE_BOKEHBLUR:
      ntype->draw_buttons = node_composit_buts_bokehblur;
      break;
    case CMP_NODE_VIEWER:
      ntype->draw_buttons = node_composit_buts_viewer;
      ntype->draw_buttons_ex = node_composit_buts_viewer_ex;
      ntype->draw_backdrop = node_composit_backdrop_viewer;
      break;
    case CMP_NODE_COMPOSITE:
      ntype->draw_buttons = node_composit_buts_composite;
      break;
    case CMP_NODE_MASK:
      ntype->draw_buttons = node_composit_buts_mask;
      break;
    case CMP_NODE_KEYINGSCREEN:
      ntype->draw_buttons = node_composit_buts_keyingscreen;
      break;
    case CMP_NODE_KEYING:
      ntype->draw_buttons = node_composit_buts_keying;
      break;
    case CMP_NODE_TRACKPOS:
      ntype->draw_buttons = node_composit_buts_trackpos;
      break;
    case CMP_NODE_PLANETRACKDEFORM:
      ntype->draw_buttons = node_composit_buts_planetrackdeform;
      break;
    case CMP_NODE_CORNERPIN:
      ntype->draw_buttons = node_composit_buts_cornerpin;
      break;
    case CMP_NODE_SUNBEAMS:
      ntype->draw_buttons = node_composit_buts_sunbeams;
      break;
    case CMP_NODE_CRYPTOMATTE:
      ntype->draw_buttons = node_composit_buts_cryptomatte;
      ntype->draw_buttons_ex = node_composit_buts_cryptomatte_ex;
      break;
    case CMP_NODE_BRIGHTCONTRAST:
      ntype->draw_buttons = node_composit_buts_brightcontrast;
      break;
    case CMP_NODE_DENOISE:
      ntype->draw_buttons = node_composit_buts_denoise;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static void node_texture_buts_bricks(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "offset", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
  uiItemR(col, ptr, "offset_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "squash", DEFAULT_FLAGS, IFACE_("Squash"), ICON_NONE);
  uiItemR(col, ptr, "squash_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);
}

static void node_texture_buts_proc(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  PointerRNA tex_ptr;
  bNode *node = ptr->data;
  ID *id = ptr->owner_id;
  Tex *tex = (Tex *)node->storage;
  uiLayout *col, *row;

  RNA_pointer_create(id, &RNA_Texture, tex, &tex_ptr);

  col = uiLayoutColumn(layout, false);

  switch (tex->type) {
    case TEX_BLEND:
      uiItemR(col, &tex_ptr, "progression", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "use_flip_axis", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      break;

    case TEX_MARBLE:
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "marble_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_basis_2", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      break;

    case TEX_MAGIC:
      uiItemR(col, &tex_ptr, "noise_depth", DEFAULT_FLAGS, NULL, ICON_NONE);
      break;

    case TEX_STUCCI:
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "stucci_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      break;

    case TEX_WOOD:
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      uiItemR(col, &tex_ptr, "wood_type", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_basis_2", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiLayoutSetActive(row, !(ELEM(tex->stype, TEX_BAND, TEX_RING)));
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      break;

    case TEX_CLOUDS:
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "cloud_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, NULL, ICON_NONE);
      uiItemR(col,
              &tex_ptr,
              "noise_depth",
              DEFAULT_FLAGS | UI_ITEM_R_EXPAND,
              IFACE_("Depth"),
              ICON_NONE);
      break;

    case TEX_DISTNOISE:
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      uiItemR(col, &tex_ptr, "noise_distortion", DEFAULT_FLAGS, "", ICON_NONE);
      break;

    case TEX_MUSGRAVE:
      uiItemR(col, &tex_ptr, "musgrave_type", DEFAULT_FLAGS, "", ICON_NONE);
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      break;
    case TEX_VORONOI:
      uiItemR(col, &tex_ptr, "distance_metric", DEFAULT_FLAGS, "", ICON_NONE);
      if (tex->vn_distm == TEX_MINKOVSKY) {
        uiItemR(col, &tex_ptr, "minkovsky_exponent", DEFAULT_FLAGS, NULL, ICON_NONE);
      }
      uiItemR(col, &tex_ptr, "color_mode", DEFAULT_FLAGS, "", ICON_NONE);
      break;
  }
}

static void node_texture_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(layout,
               C,
               ptr,
               "image",
               "IMAGE_OT_new",
               "IMAGE_OT_open",
               NULL,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               NULL);
}

static void node_texture_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = ptr->data;
  PointerRNA iuserptr;

  RNA_pointer_create(ptr->owner_id, &RNA_ImageUser, node->storage, &iuserptr);
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, 0, 0);
}

static void node_texture_buts_output(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filepath", DEFAULT_FLAGS, "", ICON_NONE);
}

/* only once called */
static void node_texture_set_butfunc(bNodeType *ntype)
{
  if (ntype->type >= TEX_NODE_PROC && ntype->type < TEX_NODE_PROC_MAX) {
    ntype->draw_buttons = node_texture_buts_proc;
  }
  else {
    switch (ntype->type) {

      case TEX_NODE_MATH:
        ntype->draw_buttons = node_buts_math;
        break;

      case TEX_NODE_MIX_RGB:
        ntype->draw_buttons = node_buts_mix_rgb;
        break;

      case TEX_NODE_VALTORGB:
        ntype->draw_buttons = node_buts_colorramp;
        break;

      case TEX_NODE_CURVE_RGB:
        ntype->draw_buttons = node_buts_curvecol;
        break;

      case TEX_NODE_CURVE_TIME:
        ntype->draw_buttons = node_buts_time;
        break;

      case TEX_NODE_TEXTURE:
        ntype->draw_buttons = node_buts_texture;
        break;

      case TEX_NODE_BRICKS:
        ntype->draw_buttons = node_texture_buts_bricks;
        break;

      case TEX_NODE_IMAGE:
        ntype->draw_buttons = node_texture_buts_image;
        ntype->draw_buttons_ex = node_texture_buts_image_ex;
        break;

      case TEX_NODE_OUTPUT:
        ntype->draw_buttons = node_texture_buts_output;
        break;
    }
  }
}

/* ****************** BUTTON CALLBACKS FOR SIMULATION NODES ***************** */

static void node_simulation_buts_particle_simulation(uiLayout *layout,
                                                     bContext *UNUSED(C),
                                                     PointerRNA *ptr)
{
  uiItemR(layout, ptr, "name", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_simulation_buts_particle_time_step_event(uiLayout *layout,
                                                          bContext *UNUSED(C),
                                                          PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_simulation_buts_particle_attribute(uiLayout *layout,
                                                    bContext *UNUSED(C),
                                                    PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_simulation_buts_set_particle_attribute(uiLayout *layout,
                                                        bContext *UNUSED(C),
                                                        PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_simulation_buts_time(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_simulation_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case SIM_NODE_PARTICLE_SIMULATION:
      ntype->draw_buttons = node_simulation_buts_particle_simulation;
      break;
    case SIM_NODE_PARTICLE_TIME_STEP_EVENT:
      ntype->draw_buttons = node_simulation_buts_particle_time_step_event;
      break;
    case SIM_NODE_PARTICLE_ATTRIBUTE:
      ntype->draw_buttons = node_simulation_buts_particle_attribute;
      break;
    case SIM_NODE_SET_PARTICLE_ATTRIBUTE:
      ntype->draw_buttons = node_simulation_buts_set_particle_attribute;
      break;
    case SIM_NODE_TIME:
      ntype->draw_buttons = node_simulation_buts_time;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR FUNCTION NODES ***************** */

static void node_function_buts_boolean_math(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_function_buts_float_compare(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_function_buts_switch(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_function_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case FN_NODE_BOOLEAN_MATH:
      ntype->draw_buttons = node_function_buts_boolean_math;
      break;
    case FN_NODE_FLOAT_COMPARE:
      ntype->draw_buttons = node_function_buts_float_compare;
      break;
    case FN_NODE_SWITCH:
      ntype->draw_buttons = node_function_buts_switch;
      break;
  }
}

/* ****** init draw callbacks for all tree types, only called in usiblender.c, once ************ */

static void node_property_update_default(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  bNode *node = ptr->data;
  ED_node_tag_update_nodetree(bmain, ntree, node);
}

static void node_socket_template_properties_update(bNodeType *ntype, bNodeSocketTemplate *stemp)
{
  StructRNA *srna = ntype->rna_ext.srna;
  PropertyRNA *prop = RNA_struct_type_find_property(srna, stemp->identifier);

  if (prop) {
    RNA_def_property_update_runtime(prop, node_property_update_default);
  }
}

static void node_template_properties_update(bNodeType *ntype)
{
  bNodeSocketTemplate *stemp;

  if (ntype->inputs) {
    for (stemp = ntype->inputs; stemp->type >= 0; stemp++) {
      node_socket_template_properties_update(ntype, stemp);
    }
  }
  if (ntype->outputs) {
    for (stemp = ntype->outputs; stemp->type >= 0; stemp++) {
      node_socket_template_properties_update(ntype, stemp);
    }
  }
}

static void node_socket_undefined_draw(bContext *UNUSED(C),
                                       uiLayout *layout,
                                       PointerRNA *UNUSED(ptr),
                                       PointerRNA *UNUSED(node_ptr),
                                       const char *UNUSED(text))
{
  uiItemL(layout, IFACE_("Undefined Socket Type"), ICON_ERROR);
}

static void node_socket_undefined_draw_color(bContext *UNUSED(C),
                                             PointerRNA *UNUSED(ptr),
                                             PointerRNA *UNUSED(node_ptr),
                                             float *r_color)
{
  r_color[0] = 1.0f;
  r_color[1] = 0.0f;
  r_color[2] = 0.0f;
  r_color[3] = 1.0f;
}

static void node_socket_undefined_interface_draw(bContext *UNUSED(C),
                                                 uiLayout *layout,
                                                 PointerRNA *UNUSED(ptr))
{
  uiItemL(layout, IFACE_("Undefined Socket Type"), ICON_ERROR);
}

static void node_socket_undefined_interface_draw_color(bContext *UNUSED(C),
                                                       PointerRNA *UNUSED(ptr),
                                                       float *r_color)
{
  r_color[0] = 1.0f;
  r_color[1] = 0.0f;
  r_color[2] = 0.0f;
  r_color[3] = 1.0f;
}

void ED_node_init_butfuncs(void)
{
  /* Fallback types for undefined tree, nodes, sockets
   * Defined in blenkernel, but not registered in type hashes.
   */

  /* default ui functions */
  NodeTypeUndefined.draw_nodetype = node_draw_default;
  NodeTypeUndefined.draw_nodetype_prepare = node_update_default;
  NodeTypeUndefined.select_area_func = node_select_area_default;
  NodeTypeUndefined.tweak_area_func = node_tweak_area_default;
  NodeTypeUndefined.draw_buttons = NULL;
  NodeTypeUndefined.draw_buttons_ex = NULL;
  NodeTypeUndefined.resize_area_func = node_resize_area_default;

  NodeSocketTypeUndefined.draw = node_socket_undefined_draw;
  NodeSocketTypeUndefined.draw_color = node_socket_undefined_draw_color;
  NodeSocketTypeUndefined.interface_draw = node_socket_undefined_interface_draw;
  NodeSocketTypeUndefined.interface_draw_color = node_socket_undefined_interface_draw_color;

  /* node type ui functions */
  NODE_TYPES_BEGIN (ntype) {
    /* default ui functions */
    ntype->draw_nodetype = node_draw_default;
    ntype->draw_nodetype_prepare = node_update_default;
    ntype->select_area_func = node_select_area_default;
    ntype->tweak_area_func = node_tweak_area_default;
    ntype->draw_buttons = NULL;
    ntype->draw_buttons_ex = NULL;
    ntype->resize_area_func = node_resize_area_default;

    node_common_set_butfunc(ntype);

    node_composit_set_butfunc(ntype);
    node_shader_set_butfunc(ntype);
    node_texture_set_butfunc(ntype);
    node_simulation_set_butfunc(ntype);
    node_function_set_butfunc(ntype);

    /* define update callbacks for socket properties */
    node_template_properties_update(ntype);
  }
  NODE_TYPES_END;

  /* tree type icons */
  ntreeType_Composite->ui_icon = ICON_NODE_COMPOSITING;
  ntreeType_Shader->ui_icon = ICON_NODE_MATERIAL;
  ntreeType_Texture->ui_icon = ICON_NODE_TEXTURE;
  ntreeType_Simulation->ui_icon = ICON_PHYSICS; /* TODO: Use correct icon. */
}

void ED_init_custom_node_type(bNodeType *ntype)
{
  /* default ui functions */
  ntype->draw_nodetype = node_draw_default;
  ntype->draw_nodetype_prepare = node_update_default;
  ntype->resize_area_func = node_resize_area_default;
  ntype->select_area_func = node_select_area_default;
  ntype->tweak_area_func = node_tweak_area_default;
}

void ED_init_custom_node_socket_type(bNodeSocketType *stype)
{
  /* default ui functions */
  stype->draw = node_socket_button_label;
}

/* maps standard socket integer type to a color */
static const float std_node_socket_colors[][4] = {
    {0.63, 0.63, 0.63, 1.0}, /* SOCK_FLOAT */
    {0.39, 0.39, 0.78, 1.0}, /* SOCK_VECTOR */
    {0.78, 0.78, 0.16, 1.0}, /* SOCK_RGBA */
    {0.39, 0.78, 0.39, 1.0}, /* SOCK_SHADER */
    {0.70, 0.65, 0.19, 1.0}, /* SOCK_BOOLEAN */
    {0.0, 0.0, 0.0, 1.0},    /*__SOCK_MESH (deprecated) */
    {0.06, 0.52, 0.15, 1.0}, /* SOCK_INT */
    {0.39, 0.39, 0.39, 1.0}, /* SOCK_STRING */
    {0.40, 0.10, 0.10, 1.0}, /* SOCK_OBJECT */
    {0.10, 0.40, 0.10, 1.0}, /* SOCK_IMAGE */
    {0.80, 0.80, 0.20, 1.0}, /* SOCK_EMITTERS */
    {0.80, 0.20, 0.80, 1.0}, /* SOCK_EVENTS */
    {0.20, 0.80, 0.80, 1.0}, /* SOCK_FORCES */
    {0.30, 0.30, 0.30, 1.0}, /* SOCK_CONTROL_FLOW */
};

/* common color callbacks for standard types */
static void std_node_socket_draw_color(bContext *UNUSED(C),
                                       PointerRNA *ptr,
                                       PointerRNA *UNUSED(node_ptr),
                                       float *r_color)
{
  bNodeSocket *sock = ptr->data;
  int type = sock->typeinfo->type;
  copy_v4_v4(r_color, std_node_socket_colors[type]);
}
static void std_node_socket_interface_draw_color(bContext *UNUSED(C),
                                                 PointerRNA *ptr,
                                                 float *r_color)
{
  bNodeSocket *sock = ptr->data;
  int type = sock->typeinfo->type;
  copy_v4_v4(r_color, std_node_socket_colors[type]);
}

/* draw function for file output node sockets,
 * displays only sub-path and format, no value button */
static void node_file_output_socket_draw(bContext *C,
                                         uiLayout *layout,
                                         PointerRNA *ptr,
                                         PointerRNA *node_ptr)
{
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  bNodeSocket *sock = ptr->data;
  uiLayout *row;
  PointerRNA inputptr, imfptr;
  int imtype;

  row = uiLayoutRow(layout, false);

  imfptr = RNA_pointer_get(node_ptr, "format");
  imtype = RNA_enum_get(&imfptr, "file_format");

  if (imtype == R_IMF_IMTYPE_MULTILAYER) {
    NodeImageMultiFileSocket *input = sock->storage;
    RNA_pointer_create(&ntree->id, &RNA_NodeOutputFileSlotLayer, input, &inputptr);

    uiItemL(row, input->layer, ICON_NONE);
  }
  else {
    NodeImageMultiFileSocket *input = sock->storage;
    PropertyRNA *imtype_prop;
    const char *imtype_name;
    uiBlock *block;
    RNA_pointer_create(&ntree->id, &RNA_NodeOutputFileSlotFile, input, &inputptr);

    uiItemL(row, input->path, ICON_NONE);

    if (!RNA_boolean_get(&inputptr, "use_node_format")) {
      imfptr = RNA_pointer_get(&inputptr, "format");
    }

    imtype_prop = RNA_struct_find_property(&imfptr, "file_format");
    RNA_property_enum_name((bContext *)C,
                           &imfptr,
                           imtype_prop,
                           RNA_property_enum_get(&imfptr, imtype_prop),
                           &imtype_name);
    block = uiLayoutGetBlock(row);
    UI_block_emboss_set(block, UI_EMBOSS_PULLDOWN);
    uiItemL(row, imtype_name, ICON_NONE);
    UI_block_emboss_set(block, UI_EMBOSS_NONE);
  }
}

static void std_node_socket_draw(
    bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, const char *text)
{
  bNode *node = node_ptr->data;
  bNodeSocket *sock = ptr->data;
  int type = sock->typeinfo->type;
  /*int subtype = sock->typeinfo->subtype;*/

  /* XXX not nice, eventually give this node its own socket type ... */
  if (node->type == CMP_NODE_OUTPUT_FILE) {
    node_file_output_socket_draw(C, layout, ptr, node_ptr);
    return;
  }

  if ((sock->in_out == SOCK_OUT) || (sock->flag & SOCK_IN_USE) || (sock->flag & SOCK_HIDE_VALUE)) {
    node_socket_button_label(C, layout, ptr, node_ptr, text);
    return;
  }

  switch (type) {
    case SOCK_FLOAT:
    case SOCK_INT:
    case SOCK_BOOLEAN:
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      break;
    case SOCK_VECTOR:
      if (sock->flag & SOCK_COMPACT) {
        uiTemplateComponentMenu(layout, ptr, "default_value", text);
      }
      else {
        if (sock->typeinfo->subtype == PROP_DIRECTION) {
          uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
        }
        else {
          uiLayout *column = uiLayoutColumn(layout, true);
          uiItemR(column, ptr, "default_value", DEFAULT_FLAGS, text, ICON_NONE);
        }
      }
      break;
    case SOCK_RGBA:
    case SOCK_STRING: {
      uiLayout *row = uiLayoutSplit(layout, 0.5f, false);
      uiItemL(row, text, 0);
      uiItemR(row, ptr, "default_value", DEFAULT_FLAGS, "", 0);
      break;
    }
    case SOCK_OBJECT: {
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      break;
    }
    case SOCK_IMAGE: {
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      break;
    }
    default:
      node_socket_button_label(C, layout, ptr, node_ptr, text);
      break;
  }
}

static void std_node_socket_interface_draw(bContext *UNUSED(C), uiLayout *layout, PointerRNA *ptr)
{
  bNodeSocket *sock = ptr->data;
  int type = sock->typeinfo->type;
  /*int subtype = sock->typeinfo->subtype;*/

  switch (type) {
    case SOCK_FLOAT: {
      uiLayout *row;
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, NULL, 0);
      row = uiLayoutRow(layout, true);
      uiItemR(row, ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), 0);
      uiItemR(row, ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), 0);
      break;
    }
    case SOCK_INT: {
      uiLayout *row;
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, NULL, 0);
      row = uiLayoutRow(layout, true);
      uiItemR(row, ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), 0);
      uiItemR(row, ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), 0);
      break;
    }
    case SOCK_VECTOR: {
      uiLayout *row;
      uiItemR(layout, ptr, "default_value", UI_ITEM_R_EXPAND, NULL, DEFAULT_FLAGS);
      row = uiLayoutRow(layout, true);
      uiItemR(row, ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), 0);
      uiItemR(row, ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), 0);
      break;
    }
    case SOCK_BOOLEAN:
    case SOCK_RGBA:
    case SOCK_STRING: {
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, NULL, 0);
      break;
    }
  }
}

void ED_init_standard_node_socket_type(bNodeSocketType *stype)
{
  stype->draw = std_node_socket_draw;
  stype->draw_color = std_node_socket_draw_color;
  stype->interface_draw = std_node_socket_interface_draw;
  stype->interface_draw_color = std_node_socket_interface_draw_color;
}

static void node_socket_virtual_draw_color(bContext *UNUSED(C),
                                           PointerRNA *UNUSED(ptr),
                                           PointerRNA *UNUSED(node_ptr),
                                           float *r_color)
{
  /* alpha = 0, empty circle */
  zero_v4(r_color);
}

void ED_init_node_socket_type_virtual(bNodeSocketType *stype)
{
  stype->draw = node_socket_button_label;
  stype->draw_color = node_socket_virtual_draw_color;
}

/* ************** Generic drawing ************** */

void draw_nodespace_back_pix(const bContext *C,
                             ARegion *region,
                             SpaceNode *snode,
                             bNodeInstanceKey parent_key)
{
  Main *bmain = CTX_data_main(C);
  bNodeInstanceKey active_viewer_key = (snode->nodetree ? snode->nodetree->active_viewer_key :
                                                          NODE_INSTANCE_KEY_NONE);
  float shuffle[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  Image *ima;
  void *lock;
  ImBuf *ibuf;

  GPU_matrix_push_projection();
  GPU_matrix_push();
  wmOrtho2_region_pixelspace(region);
  GPU_matrix_identity_set();
  ED_region_draw_cb_draw(C, region, REGION_DRAW_BACKDROP);
  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  if (!(snode->flag & SNODE_BACKDRAW) || !ED_node_is_compositor(snode)) {
    return;
  }

  if (parent_key.value != active_viewer_key.value) {
    return;
  }

  ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);
  if (ibuf) {
    float x, y;

    GPU_matrix_push_projection();
    GPU_matrix_push();

    /* somehow the offset has to be calculated inverse */
    wmOrtho2_region_pixelspace(region);

    x = (region->winx - snode->zoom * ibuf->x) / 2 + snode->xof;
    y = (region->winy - snode->zoom * ibuf->y) / 2 + snode->yof;

    if (ibuf->rect || ibuf->rect_float) {
      uchar *display_buffer = NULL;
      void *cache_handle = NULL;

      if (snode->flag & (SNODE_SHOW_R | SNODE_SHOW_G | SNODE_SHOW_B | SNODE_SHOW_ALPHA)) {

        display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);

        if (snode->flag & SNODE_SHOW_R) {
          shuffle[0] = 1.0f;
        }
        else if (snode->flag & SNODE_SHOW_G) {
          shuffle[1] = 1.0f;
        }
        else if (snode->flag & SNODE_SHOW_B) {
          shuffle[2] = 1.0f;
        }
        else {
          shuffle[3] = 1.0f;
        }

        IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR);
        GPU_shader_uniform_vector(
            state.shader, GPU_shader_get_uniform(state.shader, "shuffle"), 4, 1, shuffle);

        immDrawPixelsTex(&state,
                         x,
                         y,
                         ibuf->x,
                         ibuf->y,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         GL_NEAREST,
                         display_buffer,
                         snode->zoom,
                         snode->zoom,
                         NULL);

        GPU_shader_unbind();
      }
      else if (snode->flag & SNODE_USE_ALPHA) {
        GPU_blend(true);
        GPU_blend_set_func_separate(
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

        ED_draw_imbuf_ctx(C, ibuf, x, y, GL_NEAREST, snode->zoom, snode->zoom);

        GPU_blend(false);
      }
      else {
        ED_draw_imbuf_ctx(C, ibuf, x, y, GL_NEAREST, snode->zoom, snode->zoom);
      }

      if (cache_handle) {
        IMB_display_buffer_release(cache_handle);
      }
    }

    /** \note draw selected info on backdrop */
    if (snode->edittree) {
      bNode *node = snode->edittree->nodes.first;
      rctf *viewer_border = &snode->nodetree->viewer_border;
      while (node) {
        if (node->flag & NODE_SELECT) {
          if (node->typeinfo->draw_backdrop) {
            node->typeinfo->draw_backdrop(snode, ibuf, node, x, y);
          }
        }
        node = node->next;
      }

      if ((snode->nodetree->flag & NTREE_VIEWER_BORDER) &&
          viewer_border->xmin < viewer_border->xmax && viewer_border->ymin < viewer_border->ymax) {
        rcti pixel_border;
        BLI_rcti_init(&pixel_border,
                      x + snode->zoom * viewer_border->xmin * ibuf->x,
                      x + snode->zoom * viewer_border->xmax * ibuf->x,
                      y + snode->zoom * viewer_border->ymin * ibuf->y,
                      y + snode->zoom * viewer_border->ymax * ibuf->y);

        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
        immUniformThemeColor(TH_ACTIVE);

        immDrawBorderCorners(pos, &pixel_border, 1.0f, 1.0f);

        immUnbindProgram();
      }
    }

    GPU_matrix_pop_projection();
    GPU_matrix_pop();
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

/* return quadratic beziers points for a given nodelink and clip if v2d is not NULL. */
static bool node_link_bezier_handles(View2D *v2d,
                                     SpaceNode *snode,
                                     bNodeLink *link,
                                     float vec[4][2])
{
  float dist;
  float deltax, deltay;
  float cursor[2] = {0.0f, 0.0f};
  int toreroute, fromreroute;

  /* this function can be called with snode null (via cut_links_intersect) */
  /* XXX map snode->cursor back to view space */
  if (snode) {
    cursor[0] = snode->cursor[0] * UI_DPI_FAC;
    cursor[1] = snode->cursor[1] * UI_DPI_FAC;
  }

  /* in v0 and v3 we put begin/end points */
  if (link->fromsock) {
    vec[0][0] = link->fromsock->locx;
    vec[0][1] = link->fromsock->locy;
    fromreroute = (link->fromnode && link->fromnode->type == NODE_REROUTE);
  }
  else {
    if (snode == NULL) {
      return 0;
    }
    copy_v2_v2(vec[0], cursor);
    fromreroute = 0;
  }
  if (link->tosock) {
    vec[3][0] = link->tosock->locx;
    vec[3][1] = link->tosock->locy;
    toreroute = (link->tonode && link->tonode->type == NODE_REROUTE);
  }
  else {
    if (snode == NULL) {
      return 0;
    }
    copy_v2_v2(vec[3], cursor);
    toreroute = 0;
  }

  /* may be called outside of drawing (so pass spacetype) */
  int curving = UI_GetThemeValueType(TH_NODE_CURVING, SPACE_NODE);

  if (curving == 0) {
    /* Straight line: align all points. */
    mid_v2_v2v2(vec[1], vec[0], vec[3]);
    mid_v2_v2v2(vec[2], vec[1], vec[3]);
    return 1;
  }

  dist = curving * 0.10f * fabsf(vec[0][0] - vec[3][0]);
  deltax = vec[3][0] - vec[0][0];
  deltay = vec[3][1] - vec[0][1];
  /* check direction later, for top sockets */
  if (fromreroute) {
    if (fabsf(deltax) > fabsf(deltay)) {
      vec[1][1] = vec[0][1];
      vec[1][0] = vec[0][0] + (deltax > 0 ? dist : -dist);
    }
    else {
      vec[1][0] = vec[0][0];
      vec[1][1] = vec[0][1] + (deltay > 0 ? dist : -dist);
    }
  }
  else {
    vec[1][0] = vec[0][0] + dist;
    vec[1][1] = vec[0][1];
  }
  if (toreroute) {
    if (fabsf(deltax) > fabsf(deltay)) {
      vec[2][1] = vec[3][1];
      vec[2][0] = vec[3][0] + (deltax > 0 ? -dist : dist);
    }
    else {
      vec[2][0] = vec[3][0];
      vec[2][1] = vec[3][1] + (deltay > 0 ? -dist : dist);
    }
  }
  else {
    vec[2][0] = vec[3][0] - dist;
    vec[2][1] = vec[3][1];
  }

  if (v2d && min_ffff(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) > v2d->cur.xmax) {
    return 0; /* clipped */
  }
  else if (v2d && max_ffff(vec[0][0], vec[1][0], vec[2][0], vec[3][0]) < v2d->cur.xmin) {
    return 0; /* clipped */
  }

  return 1;
}

/* if v2d not NULL, it clips and returns 0 if not visible */
bool node_link_bezier_points(
    View2D *v2d, SpaceNode *snode, bNodeLink *link, float coord_array[][2], int resol)
{
  float vec[4][2];

  if (node_link_bezier_handles(v2d, snode, link, vec)) {
    /* always do all three, to prevent data hanging around */
    BKE_curve_forward_diff_bezier(
        vec[0][0], vec[1][0], vec[2][0], vec[3][0], coord_array[0] + 0, resol, sizeof(float) * 2);
    BKE_curve_forward_diff_bezier(
        vec[0][1], vec[1][1], vec[2][1], vec[3][1], coord_array[0] + 1, resol, sizeof(float) * 2);

    return 1;
  }
  return 0;
}

#define NODELINK_GROUP_SIZE 256
#define LINK_RESOL 24
#define LINK_WIDTH (2.5f * UI_DPI_FAC)
#define ARROW_SIZE (7 * UI_DPI_FAC)

static float arrow_verts[3][2] = {{-1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, -1.0f}};
static float arrow_expand_axis[3][2] = {{0.7071f, 0.7071f}, {M_SQRT2, 0.0f}, {0.7071f, -0.7071f}};

static struct {
  GPUBatch *batch;        /* for batching line together */
  GPUBatch *batch_single; /* for single line */
  GPUVertBuf *inst_vbo;
  uint p0_id, p1_id, p2_id, p3_id;
  uint colid_id;
  GPUVertBufRaw p0_step, p1_step, p2_step, p3_step;
  GPUVertBufRaw colid_step;
  uint count;
  bool enabled;
} g_batch_link = {0};

static void nodelink_batch_reset(void)
{
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p0_id, &g_batch_link.p0_step);
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p1_id, &g_batch_link.p1_step);
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p2_id, &g_batch_link.p2_step);
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p3_id, &g_batch_link.p3_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.colid_id, &g_batch_link.colid_step);
  g_batch_link.count = 0;
}

static void set_nodelink_vertex(GPUVertBuf *vbo,
                                uint uv_id,
                                uint pos_id,
                                uint exp_id,
                                uint v,
                                const uchar uv[2],
                                const float pos[2],
                                const float exp[2])
{
  GPU_vertbuf_attr_set(vbo, uv_id, v, uv);
  GPU_vertbuf_attr_set(vbo, pos_id, v, pos);
  GPU_vertbuf_attr_set(vbo, exp_id, v, exp);
}

static void nodelink_batch_init(void)
{
  GPUVertFormat format = {0};
  uint uv_id = GPU_vertformat_attr_add(&format, "uv", GPU_COMP_U8, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
  uint pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint expand_id = GPU_vertformat_attr_add(&format, "expand", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_STATIC);
  int vcount = LINK_RESOL * 2; /* curve */
  vcount += 2;                 /* restart strip */
  vcount += 3 * 2;             /* arrow */
  vcount *= 2;                 /* shadow */
  vcount += 2;                 /* restart strip */
  GPU_vertbuf_data_alloc(vbo, vcount);
  int v = 0;

  for (int k = 0; k < 2; k++) {
    uchar uv[2] = {0, 0};
    float pos[2] = {0.0f, 0.0f};
    float exp[2] = {0.0f, 1.0f};

    /* restart */
    if (k == 1) {
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }

    /* curve strip */
    for (int i = 0; i < LINK_RESOL; i++) {
      uv[0] = 255 * (i / (float)(LINK_RESOL - 1));
      uv[1] = 0;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
      uv[1] = 255;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }
    /* restart */
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

    uv[0] = 127;
    uv[1] = 0;
    copy_v2_v2(pos, arrow_verts[0]);
    copy_v2_v2(exp, arrow_expand_axis[0]);
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    /* arrow */
    for (int i = 0; i < 3; i++) {
      uv[1] = 0;
      copy_v2_v2(pos, arrow_verts[i]);
      copy_v2_v2(exp, arrow_expand_axis[i]);
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

      uv[1] = 255;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }

    /* restart */
    if (k == 0) {
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }
  }

  g_batch_link.batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
  gpu_batch_presets_register(g_batch_link.batch);

  g_batch_link.batch_single = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, 0);
  gpu_batch_presets_register(g_batch_link.batch_single);

  /* Instances data */
  GPUVertFormat format_inst = {0};
  g_batch_link.p0_id = GPU_vertformat_attr_add(
      &format_inst, "P0", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  g_batch_link.p1_id = GPU_vertformat_attr_add(
      &format_inst, "P1", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  g_batch_link.p2_id = GPU_vertformat_attr_add(
      &format_inst, "P2", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  g_batch_link.p3_id = GPU_vertformat_attr_add(
      &format_inst, "P3", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  g_batch_link.colid_id = GPU_vertformat_attr_add(
      &format_inst, "colid_doarrow", GPU_COMP_U8, 4, GPU_FETCH_INT);
  g_batch_link.inst_vbo = GPU_vertbuf_create_with_format_ex(&format_inst, GPU_USAGE_STREAM);
  /* Alloc max count but only draw the range we need. */
  GPU_vertbuf_data_alloc(g_batch_link.inst_vbo, NODELINK_GROUP_SIZE);

  GPU_batch_instbuf_set(g_batch_link.batch, g_batch_link.inst_vbo, true);

  nodelink_batch_reset();
}

static char nodelink_get_color_id(int th_col)
{
  switch (th_col) {
    case TH_WIRE:
      return 1;
    case TH_WIRE_INNER:
      return 2;
    case TH_ACTIVE:
      return 3;
    case TH_EDGE_SELECT:
      return 4;
    case TH_REDALERT:
      return 5;
  }
  return 0;
}

static void nodelink_batch_draw(SpaceNode *snode)
{
  if (g_batch_link.count == 0) {
    return;
  }

  GPU_blend(true);

  float colors[6][4] = {{0.0f}};
  UI_GetThemeColor4fv(TH_WIRE_INNER, colors[nodelink_get_color_id(TH_WIRE_INNER)]);
  UI_GetThemeColor4fv(TH_WIRE, colors[nodelink_get_color_id(TH_WIRE)]);
  UI_GetThemeColor4fv(TH_ACTIVE, colors[nodelink_get_color_id(TH_ACTIVE)]);
  UI_GetThemeColor4fv(TH_EDGE_SELECT, colors[nodelink_get_color_id(TH_EDGE_SELECT)]);
  UI_GetThemeColor4fv(TH_REDALERT, colors[nodelink_get_color_id(TH_REDALERT)]);

  GPU_vertbuf_data_len_set(g_batch_link.inst_vbo, g_batch_link.count);
  GPU_vertbuf_use(g_batch_link.inst_vbo); /* force update. */

  GPU_batch_program_set_builtin(g_batch_link.batch, GPU_SHADER_2D_NODELINK_INST);
  GPU_batch_uniform_4fv_array(g_batch_link.batch, "colors", 6, (float *)colors);
  GPU_batch_uniform_1f(g_batch_link.batch, "expandSize", snode->aspect * LINK_WIDTH);
  GPU_batch_uniform_1f(g_batch_link.batch, "arrowSize", ARROW_SIZE);
  GPU_batch_draw(g_batch_link.batch);

  nodelink_batch_reset();

  GPU_blend(false);
}

void nodelink_batch_start(SpaceNode *UNUSED(snode))
{
  g_batch_link.enabled = true;
}

void nodelink_batch_end(SpaceNode *snode)
{
  nodelink_batch_draw(snode);
  g_batch_link.enabled = false;
}

static void nodelink_batch_add_link(SpaceNode *snode,
                                    const float p0[2],
                                    const float p1[2],
                                    const float p2[2],
                                    const float p3[2],
                                    int th_col1,
                                    int th_col2,
                                    int th_col3,
                                    bool drawarrow)
{
  /* Only allow these colors. If more is needed, you need to modify the shader accordingly. */
  BLI_assert(ELEM(th_col1, TH_WIRE_INNER, TH_WIRE, TH_ACTIVE, TH_EDGE_SELECT, TH_REDALERT));
  BLI_assert(ELEM(th_col2, TH_WIRE_INNER, TH_WIRE, TH_ACTIVE, TH_EDGE_SELECT, TH_REDALERT));
  BLI_assert(ELEM(th_col3, TH_WIRE, -1));

  g_batch_link.count++;
  copy_v2_v2(GPU_vertbuf_raw_step(&g_batch_link.p0_step), p0);
  copy_v2_v2(GPU_vertbuf_raw_step(&g_batch_link.p1_step), p1);
  copy_v2_v2(GPU_vertbuf_raw_step(&g_batch_link.p2_step), p2);
  copy_v2_v2(GPU_vertbuf_raw_step(&g_batch_link.p3_step), p3);
  char *colid = GPU_vertbuf_raw_step(&g_batch_link.colid_step);
  colid[0] = nodelink_get_color_id(th_col1);
  colid[1] = nodelink_get_color_id(th_col2);
  colid[2] = nodelink_get_color_id(th_col3);
  colid[3] = drawarrow;

  if (g_batch_link.count == NODELINK_GROUP_SIZE) {
    nodelink_batch_draw(snode);
  }
}

/* don't do shadows if th_col3 is -1. */
void node_draw_link_bezier(
    View2D *v2d, SpaceNode *snode, bNodeLink *link, int th_col1, int th_col2, int th_col3)
{
  float vec[4][2];

  if (node_link_bezier_handles(v2d, snode, link, vec)) {
    int drawarrow = ((link->tonode && (link->tonode->type == NODE_REROUTE)) &&
                     (link->fromnode && (link->fromnode->type == NODE_REROUTE)));

    if (g_batch_link.batch == NULL) {
      nodelink_batch_init();
    }

    if (g_batch_link.enabled) {
      /* Add link to batch. */
      nodelink_batch_add_link(
          snode, vec[0], vec[1], vec[2], vec[3], th_col1, th_col2, th_col3, drawarrow);
    }
    else {
      /* Draw single link. */
      float colors[3][4] = {{0.0f}};
      if (th_col3 != -1) {
        UI_GetThemeColor4fv(th_col3, colors[0]);
      }
      UI_GetThemeColor4fv(th_col1, colors[1]);
      UI_GetThemeColor4fv(th_col2, colors[2]);

      GPUBatch *batch = g_batch_link.batch_single;
      GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_NODELINK);
      GPU_batch_uniform_2fv_array(batch, "bezierPts", 4, (float *)vec);
      GPU_batch_uniform_4fv_array(batch, "colors", 3, (float *)colors);
      GPU_batch_uniform_1f(batch, "expandSize", snode->aspect * LINK_WIDTH);
      GPU_batch_uniform_1f(batch, "arrowSize", ARROW_SIZE);
      GPU_batch_uniform_1i(batch, "doArrow", drawarrow);
      GPU_batch_draw(batch);
    }
  }
}

/* note; this is used for fake links in groups too */
void node_draw_link(View2D *v2d, SpaceNode *snode, bNodeLink *link)
{
  int th_col1 = TH_WIRE_INNER, th_col2 = TH_WIRE_INNER, th_col3 = TH_WIRE;

  if (link->fromsock == NULL && link->tosock == NULL) {
    return;
  }

  /* new connection */
  if (!link->fromsock || !link->tosock) {
    th_col1 = th_col2 = TH_ACTIVE;
  }
  else {
    /* going to give issues once... */
    if (link->tosock->flag & SOCK_UNAVAIL) {
      return;
    }
    if (link->fromsock->flag & SOCK_UNAVAIL) {
      return;
    }

    if (link->flag & NODE_LINK_VALID) {
      /* special indicated link, on drop-node */
      if (link->flag & NODE_LINKFLAG_HILITE) {
        th_col1 = th_col2 = TH_ACTIVE;
      }
      else {
        /* regular link */
        if (link->fromnode && link->fromnode->flag & SELECT) {
          th_col1 = TH_EDGE_SELECT;
        }
        if (link->tonode && link->tonode->flag & SELECT) {
          th_col2 = TH_EDGE_SELECT;
        }
      }
    }
    else {
      th_col1 = th_col2 = TH_REDALERT;
      // th_col3 = -1; /* no shadow */
    }
  }

  node_draw_link_bezier(v2d, snode, link, th_col1, th_col2, th_col3);
  //  node_draw_link_straight(v2d, snode, link, th_col1, do_shaded, th_col2, do_triple, th_col3);
}

void ED_node_draw_snap(View2D *v2d, const float cent[2], float size, NodeBorder border, uint pos)
{
  immBegin(GPU_PRIM_LINES, 4);

  if (border & (NODE_LEFT | NODE_RIGHT)) {
    immVertex2f(pos, cent[0], v2d->cur.ymin);
    immVertex2f(pos, cent[0], v2d->cur.ymax);
  }
  else {
    immVertex2f(pos, cent[0], cent[1] - size);
    immVertex2f(pos, cent[0], cent[1] + size);
  }

  if (border & (NODE_TOP | NODE_BOTTOM)) {
    immVertex2f(pos, v2d->cur.xmin, cent[1]);
    immVertex2f(pos, v2d->cur.xmax, cent[1]);
  }
  else {
    immVertex2f(pos, cent[0] - size, cent[1]);
    immVertex2f(pos, cent[0] + size, cent[1]);
  }

  immEnd();
}
