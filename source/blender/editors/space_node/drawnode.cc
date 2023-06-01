/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 * \brief lower level node drawing for nodes (boarders, headers etc), also node layout.
 */

#include "BLI_color.hh"
#include "BLI_system.h"
#include "BLI_threads.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_scene.h"
#include "BKE_tracking.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "BIF_glutil.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader_shared.h"
#include "GPU_state.h"
#include "GPU_uniform_buffer.h"

#include "DRW_engine.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "ED_node.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "NOD_composite.h"
#include "NOD_geometry.h"
#include "NOD_node_declaration.hh"
#include "NOD_shader.h"
#include "NOD_texture.h"
#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

/* Default flags for uiItemR(). Name is kept short since this is used a lot in this file. */
#define DEFAULT_FLAGS UI_ITEM_R_SPLIT_EMPTY_NAME

/* ****************** SOCKET BUTTON DRAW FUNCTIONS ***************** */

static void node_socket_button_label(bContext * /*C*/,
                                     uiLayout *layout,
                                     PointerRNA * /*ptr*/,
                                     PointerRNA * /*node_ptr*/,
                                     const char *text)
{
  uiItemL(layout, text, 0);
}

/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

static void node_buts_value(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  /* first output stores value */
  bNodeSocket *output = (bNodeSocket *)node->outputs.first;
  PointerRNA sockptr;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, output, &sockptr);

  uiItemR(layout, &sockptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_rgb(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  /* first output stores value */
  bNodeSocket *output = (bNodeSocket *)node->outputs.first;
  PointerRNA sockptr;
  uiLayout *col;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, output, &sockptr);

  col = uiLayoutColumn(layout, false);
  uiTemplateColorPicker(col, &sockptr, "default_value", true, false, false, false);
  uiItemR(col, &sockptr, "default_value", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, "", ICON_NONE);
}

static void node_buts_mix_rgb(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;

  uiLayout *col = uiLayoutColumn(layout, false);
  uiLayout *row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "blend_type", DEFAULT_FLAGS, "", ICON_NONE);
  if (ELEM(ntree->type, NTREE_COMPOSIT, NTREE_TEXTURE)) {
    uiItemR(row, ptr, "use_alpha", DEFAULT_FLAGS, "", ICON_IMAGE_RGB_ALPHA);
  }

  uiItemR(col, ptr, "use_clamp", DEFAULT_FLAGS, nullptr, ICON_NONE);
}

static void node_buts_time(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "curve", 's', false, false, false, false);

  uiLayout *col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "frame_start", DEFAULT_FLAGS, IFACE_("Start"), ICON_NONE);
  uiItemR(col, ptr, "frame_end", DEFAULT_FLAGS, IFACE_("End"), ICON_NONE);
}

static void node_buts_colorramp(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateColorRamp(layout, ptr, "color_ramp", false);
}

static void node_buts_curvevec(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 'v', false, false, false, false);
}

static void node_buts_curvefloat(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 0, false, false, false, false);
}

}  // namespace blender::ed::space_node

#define SAMPLE_FLT_ISNONE FLT_MAX
/* Bad! 2.5 will do better? ... no it won't! */
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

namespace blender::ed::space_node {

static void node_buts_curvecol(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  CurveMapping *cumap = (CurveMapping *)node->storage;

  if (_sample_col[0] != SAMPLE_FLT_ISNONE) {
    cumap->flag |= CUMA_DRAW_SAMPLE;
    copy_v3_v3(cumap->sample, _sample_col);
  }
  else {
    cumap->flag &= ~CUMA_DRAW_SAMPLE;
  }

  /* "Tone" (Standard/Film-like) only used in the Compositor. */
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  uiTemplateCurveMapping(
      layout, ptr, "mapping", 'c', false, false, false, (ntree->type == NTREE_COMPOSIT));
}

static void node_buts_normal(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  /* first output stores normal */
  bNodeSocket *output = (bNodeSocket *)node->outputs.first;
  PointerRNA sockptr;
  RNA_pointer_create(ptr->owner_id, &RNA_NodeSocket, output, &sockptr);

  uiItemR(layout, &sockptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_texture(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  short multi = (node->id && ((Tex *)node->id)->use_nodes && (node->type != CMP_NODE_TEXTURE) &&
                 (node->type != TEX_NODE_TEXTURE));

  uiItemR(layout, ptr, "texture", DEFAULT_FLAGS, "", ICON_NONE);

  if (multi) {
    /* Number Drawing not optimal here, better have a list. */
    uiItemR(layout, ptr, "node_output", DEFAULT_FLAGS, "", ICON_NONE);
  }
}

static void node_buts_math(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "use_clamp", DEFAULT_FLAGS, nullptr, ICON_NONE);
}

static void node_buts_combsep_color(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

NodeResizeDirection node_get_resize_direction(const bNode *node, const int x, const int y)
{
  if (node->type == NODE_FRAME) {
    NodeFrame *data = (NodeFrame *)node->storage;

    /* shrinking frame size is determined by child nodes */
    if (!(data->flag & NODE_FRAME_RESIZEABLE)) {
      return NODE_RESIZE_NONE;
    }

    NodeResizeDirection dir = NODE_RESIZE_NONE;

    const rctf &totr = node->runtime->totr;
    const float size = NODE_RESIZE_MARGIN;

    if (x > totr.xmax - size && x <= totr.xmax && y >= totr.ymin && y < totr.ymax) {
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

  if (node->flag & NODE_HIDDEN) {
    /* right part of node */
    rctf totr = node->runtime->totr;
    totr.xmin = node->runtime->totr.xmax - 1.0f * U.widget_unit;
    if (BLI_rctf_isect_pt(&totr, x, y)) {
      return NODE_RESIZE_RIGHT;
    }

    return NODE_RESIZE_NONE;
  }

  const float size = NODE_RESIZE_MARGIN;
  const rctf &totr = node->runtime->totr;
  NodeResizeDirection dir = NODE_RESIZE_NONE;

  if (x >= totr.xmax - size && x < totr.xmax && y >= totr.ymin && y < totr.ymax) {
    dir |= NODE_RESIZE_RIGHT;
  }
  if (x >= totr.xmin && x < totr.xmin + size && y >= totr.ymin && y < totr.ymax) {
    dir |= NODE_RESIZE_LEFT;
  }
  return dir;
}

/* ****************** BUTTON CALLBACKS FOR COMMON NODES ***************** */

static void node_draw_buttons_group(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateIDBrowse(
      layout, C, ptr, "node_tree", nullptr, nullptr, nullptr, UI_TEMPLATE_ID_FILTER_ALL, nullptr);
}

static void node_buts_frame_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "label_size", DEFAULT_FLAGS, IFACE_("Label Size"), ICON_NONE);
  uiItemR(layout, ptr, "shrink", DEFAULT_FLAGS, IFACE_("Shrink"), ICON_NONE);
  uiItemR(layout, ptr, "text", DEFAULT_FLAGS, nullptr, ICON_NONE);
}

static void node_common_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case NODE_GROUP:
      ntype->draw_buttons = node_draw_buttons_group;
      break;
    case NODE_FRAME:
      ntype->draw_buttons_ex = node_buts_frame_ex;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR SHADER NODES ***************** */

static void node_buts_image_user(uiLayout *layout,
                                 bContext *C,
                                 PointerRNA *ptr,
                                 PointerRNA *imaptr,
                                 PointerRNA *iuserptr,
                                 const bool show_layer_selection,
                                 const bool show_color_management)
{
  Image *image = (Image *)imaptr->data;
  if (!image) {
    return;
  }
  ImageUser *iuser = (ImageUser *)iuserptr->data;

  uiLayout *col = uiLayoutColumn(layout, false);

  uiItemR(col, imaptr, "source", DEFAULT_FLAGS, "", ICON_NONE);

  const int source = RNA_enum_get(imaptr, "source");

  if (source == IMA_SRC_SEQUENCE) {
    /* don't use iuser->framenr directly
     * because it may not be updated if auto-refresh is off */
    Scene *scene = CTX_data_scene(C);

    char numstr[32];
    const int framenr = BKE_image_user_frame_get(iuser, scene->r.cfra, nullptr);
    SNPRINTF(numstr, IFACE_("Frame: %d"), framenr);
    uiItemL(layout, numstr, ICON_NONE);
  }

  if (ELEM(source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "frame_duration", DEFAULT_FLAGS, nullptr, ICON_NONE);
    uiItemR(col, ptr, "frame_start", DEFAULT_FLAGS, nullptr, ICON_NONE);
    uiItemR(col, ptr, "frame_offset", DEFAULT_FLAGS, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_cyclic", DEFAULT_FLAGS, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_auto_refresh", DEFAULT_FLAGS, nullptr, ICON_NONE);
  }

  if (show_layer_selection && RNA_enum_get(imaptr, "type") == IMA_TYPE_MULTILAYER &&
      RNA_boolean_get(ptr, "has_layers"))
  {
    col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "layer", DEFAULT_FLAGS, nullptr, ICON_NONE);
  }

  if (show_color_management) {
    uiLayout *split = uiLayoutSplit(layout, 0.33f, true);
    PointerRNA colorspace_settings_ptr = RNA_pointer_get(imaptr, "colorspace_settings");
    uiItemL(split, IFACE_("Color Space"), ICON_NONE);
    uiItemR(split, &colorspace_settings_ptr, "name", DEFAULT_FLAGS, "", ICON_NONE);

    if (image->source != IMA_SRC_GENERATED) {
      split = uiLayoutSplit(layout, 0.33f, true);
      uiItemL(split, IFACE_("Alpha"), ICON_NONE);
      uiItemR(split, imaptr, "alpha_mode", DEFAULT_FLAGS, "", ICON_NONE);

      bool is_data = IMB_colormanagement_space_name_is_data(image->colorspace_settings.name);
      uiLayoutSetActive(split, !is_data);
    }

    /* Avoid losing changes image is painted. */
    if (BKE_image_is_dirty((Image *)imaptr->data)) {
      uiLayoutSetEnabled(split, false);
    }
  }
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
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "projection", DEFAULT_FLAGS, "", ICON_NONE);

  if (RNA_enum_get(ptr, "projection") == SHD_PROJ_BOX) {
    uiItemR(layout, ptr, "projection_blend", DEFAULT_FLAGS, "Blend", ICON_NONE);
  }

  uiItemR(layout, ptr, "extension", DEFAULT_FLAGS, "", ICON_NONE);

  /* NOTE: image user properties used directly here, unlike compositor image node,
   * which redefines them in the node struct RNA to get proper updates.
   */
  node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr, false, true);
}

static void node_shader_buts_tex_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, false, false);
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
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, "", ICON_NONE);
  uiItemR(layout, ptr, "projection", DEFAULT_FLAGS, "", ICON_NONE);

  node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr, false, true);
}

static void node_shader_buts_tex_environment_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, false, false);

  uiItemR(layout, ptr, "interpolation", DEFAULT_FLAGS, IFACE_("Interpolation"), ICON_NONE);
  uiItemR(layout, ptr, "projection", DEFAULT_FLAGS, IFACE_("Projection"), ICON_NONE);
}

static void node_shader_buts_displacement(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "space", DEFAULT_FLAGS, "", 0);
}

static void node_shader_buts_glossy(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_output_shader(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "target", DEFAULT_FLAGS, "", ICON_NONE);
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
    case SH_NODE_CURVE_FLOAT:
      ntype->draw_buttons = node_buts_curvefloat;
      break;
    case SH_NODE_VALUE:
      ntype->draw_buttons = node_buts_value;
      break;
    case SH_NODE_RGB:
      ntype->draw_buttons = node_buts_rgb;
      break;
    case SH_NODE_MIX_RGB_LEGACY:
      ntype->draw_buttons = node_buts_mix_rgb;
      break;
    case SH_NODE_VALTORGB:
      ntype->draw_buttons = node_buts_colorramp;
      break;
    case SH_NODE_MATH:
      ntype->draw_buttons = node_buts_math;
      break;
    case SH_NODE_COMBINE_COLOR:
    case SH_NODE_SEPARATE_COLOR:
      ntype->draw_buttons = node_buts_combsep_color;
      break;
    case SH_NODE_TEX_IMAGE:
      ntype->draw_buttons = node_shader_buts_tex_image;
      ntype->draw_buttons_ex = node_shader_buts_tex_image_ex;
      break;
    case SH_NODE_TEX_ENVIRONMENT:
      ntype->draw_buttons = node_shader_buts_tex_environment;
      ntype->draw_buttons_ex = node_shader_buts_tex_environment_ex;
      break;
    case SH_NODE_DISPLACEMENT:
    case SH_NODE_VECTOR_DISPLACEMENT:
      ntype->draw_buttons = node_shader_buts_displacement;
      break;
    case SH_NODE_BSDF_GLASS:
    case SH_NODE_BSDF_REFRACTION:
      ntype->draw_buttons = node_shader_buts_glossy;
      break;
    case SH_NODE_OUTPUT_MATERIAL:
    case SH_NODE_OUTPUT_LIGHT:
    case SH_NODE_OUTPUT_WORLD:
      ntype->draw_buttons = node_buts_output_shader;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR COMPOSITE NODES ***************** */

static void node_buts_image_views(uiLayout *layout,
                                  bContext * /*C*/,
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
      uiItemR(col, ptr, "view", DEFAULT_FLAGS, nullptr, ICON_CAMERA_STEREO);
    }
    else {
      uiItemR(col, ptr, "view", DEFAULT_FLAGS, nullptr, ICON_SCENE);
    }
  }
}

static void node_composit_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  PointerRNA iuserptr;
  RNA_pointer_create(ptr->owner_id, &RNA_ImageUser, node->storage, &iuserptr);
  uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
  uiTemplateID(layout,
               C,
               ptr,
               "image",
               "IMAGE_OT_new",
               "IMAGE_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
  if (!node->id) {
    return;
  }

  PointerRNA imaptr = RNA_pointer_get(ptr, "image");

  node_buts_image_user(layout, C, ptr, &imaptr, &iuserptr, true, true);

  node_buts_image_views(layout, C, ptr, &imaptr);
}

static void node_composit_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  PointerRNA iuserptr;
  RNA_pointer_create(ptr->owner_id, &RNA_ImageUser, node->storage, &iuserptr);
  uiLayoutSetContextPointer(layout, "image_user", &iuserptr);
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, false, true);
}

static void node_composit_buts_huecorrect(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  CurveMapping *cumap = (CurveMapping *)node->storage;

  if (_sample_col[0] != SAMPLE_FLT_ISNONE) {
    cumap->flag |= CUMA_DRAW_SAMPLE;
    copy_v3_v3(cumap->sample, _sample_col);
  }
  else {
    cumap->flag &= ~CUMA_DRAW_SAMPLE;
  }

  uiTemplateCurveMapping(layout, ptr, "mapping", 'h', false, false, false, false);
}

static void node_composit_buts_ycc(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_combsep_color(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;

  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
  if (storage->mode == CMP_NODE_COMBSEP_COLOR_YCC) {
    uiItemR(layout, ptr, "ycc_mode", DEFAULT_FLAGS, "", ICON_NONE);
  }
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

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

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
  NodeBoxMask *boxmask = (NodeBoxMask *)node->storage;
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

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

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
  NodeEllipseMask *ellipsemask = (NodeEllipseMask *)node->storage;
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

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  immUniformColor3f(1.0f, 1.0f, 1.0f);

  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x3, y3);
  immVertex2f(pos, x4, y4);
  immEnd();

  immUnbindProgram();
}

static void node_composit_buts_cryptomatte_legacy(uiLayout *layout,
                                                  bContext * /*C*/,
                                                  PointerRNA *ptr)
{
  uiLayout *col = uiLayoutColumn(layout, true);

  uiItemL(col, IFACE_("Matte Objects:"), ICON_NONE);

  uiLayout *row = uiLayoutRow(col, true);
  uiTemplateCryptoPicker(row, ptr, "add", ICON_ADD);
  uiTemplateCryptoPicker(row, ptr, "remove", ICON_REMOVE);

  uiItemR(col, ptr, "matte_id", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_cryptomatte_legacy_ex(uiLayout *layout,
                                                     bContext * /*C*/,
                                                     PointerRNA * /*ptr*/)
{
  uiItemO(layout, IFACE_("Add Crypto Layer"), ICON_ADD, "NODE_OT_cryptomatte_layer_add");
  uiItemO(layout, IFACE_("Remove Crypto Layer"), ICON_REMOVE, "NODE_OT_cryptomatte_layer_remove");
}

static void node_composit_buts_cryptomatte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiLayout *row = uiLayoutRow(layout, true);
  uiItemR(row, ptr, "source", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayout *col = uiLayoutColumn(layout, false);
  if (node->custom1 == CMP_CRYPTOMATTE_SRC_RENDER) {
    uiTemplateID(col,
                 C,
                 ptr,
                 "scene",
                 nullptr,
                 nullptr,
                 nullptr,
                 UI_TEMPLATE_ID_FILTER_ALL,
                 false,
                 nullptr);
  }
  else {
    uiTemplateID(col,
                 C,
                 ptr,
                 "image",
                 nullptr,
                 "IMAGE_OT_open",
                 nullptr,
                 UI_TEMPLATE_ID_FILTER_ALL,
                 false,
                 nullptr);

    NodeCryptomatte *crypto = (NodeCryptomatte *)node->storage;
    PointerRNA imaptr = RNA_pointer_get(ptr, "image");
    PointerRNA iuserptr;
    RNA_pointer_create((ID *)ptr->owner_id, &RNA_ImageUser, &crypto->iuser, &iuserptr);
    uiLayoutSetContextPointer(layout, "image_user", &iuserptr);

    node_buts_image_user(col, C, ptr, &imaptr, &iuserptr, false, false);
    node_buts_image_views(col, C, ptr, &imaptr);
  }

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "layer_name", 0, "", ICON_NONE);
  uiItemL(col, IFACE_("Matte ID:"), ICON_NONE);

  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "matte_id", DEFAULT_FLAGS, "", ICON_NONE);
  uiTemplateCryptoPicker(row, ptr, "add", ICON_ADD);
  uiTemplateCryptoPicker(row, ptr, "remove", ICON_REMOVE);
}

/* only once called */
static void node_composit_set_butfunc(bNodeType *ntype)
{
  switch (ntype->type) {
    case CMP_NODE_IMAGE:
      ntype->draw_buttons = node_composit_buts_image;
      ntype->draw_buttons_ex = node_composit_buts_image_ex;
      break;
    case CMP_NODE_NORMAL:
      ntype->draw_buttons = node_buts_normal;
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
    case CMP_NODE_MIX_RGB:
      ntype->draw_buttons = node_buts_mix_rgb;
      break;
    case CMP_NODE_VALTORGB:
      ntype->draw_buttons = node_buts_colorramp;
      break;
    case CMP_NODE_TIME:
      ntype->draw_buttons = node_buts_time;
      break;
    case CMP_NODE_TEXTURE:
      ntype->draw_buttons = node_buts_texture;
      break;
    case CMP_NODE_MATH:
      ntype->draw_buttons = node_buts_math;
      break;
    case CMP_NODE_HUECORRECT:
      ntype->draw_buttons = node_composit_buts_huecorrect;
      break;
    case CMP_NODE_COMBINE_COLOR:
    case CMP_NODE_SEPARATE_COLOR:
      ntype->draw_buttons = node_composit_buts_combsep_color;
      break;
    case CMP_NODE_COMBYCCA_LEGACY:
    case CMP_NODE_SEPYCCA_LEGACY:
      ntype->draw_buttons = node_composit_buts_ycc;
      break;
    case CMP_NODE_MASK_BOX:
      ntype->draw_backdrop = node_composit_backdrop_boxmask;
      break;
    case CMP_NODE_MASK_ELLIPSE:
      ntype->draw_backdrop = node_composit_backdrop_ellipsemask;
      break;
    case CMP_NODE_CRYPTOMATTE:
      ntype->draw_buttons = node_composit_buts_cryptomatte;
      break;
    case CMP_NODE_CRYPTOMATTE_LEGACY:
      ntype->draw_buttons = node_composit_buts_cryptomatte_legacy;
      ntype->draw_buttons_ex = node_composit_buts_cryptomatte_legacy_ex;
      break;
    case CMP_NODE_VIEWER:
      ntype->draw_backdrop = node_composit_backdrop_viewer;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static void node_texture_buts_bricks(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "offset", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
  uiItemR(col, ptr, "offset_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "squash", DEFAULT_FLAGS, IFACE_("Squash"), ICON_NONE);
  uiItemR(col, ptr, "squash_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);
}

static void node_texture_buts_proc(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  PointerRNA tex_ptr;
  bNode *node = (bNode *)ptr->data;
  ID *id = ptr->owner_id;
  Tex *tex = (Tex *)node->storage;
  uiLayout *col, *row;

  RNA_pointer_create(id, &RNA_Texture, tex, &tex_ptr);

  col = uiLayoutColumn(layout, false);

  switch (tex->type) {
    case TEX_BLEND:
      uiItemR(col, &tex_ptr, "progression", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(
          row, &tex_ptr, "use_flip_axis", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      break;

    case TEX_MARBLE:
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "marble_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(
          row, &tex_ptr, "noise_basis_2", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      break;

    case TEX_MAGIC:
      uiItemR(col, &tex_ptr, "noise_depth", DEFAULT_FLAGS, nullptr, ICON_NONE);
      break;

    case TEX_STUCCI:
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "stucci_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      break;

    case TEX_WOOD:
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      uiItemR(col, &tex_ptr, "wood_type", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(
          row, &tex_ptr, "noise_basis_2", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiLayoutSetActive(row, !ELEM(tex->stype, TEX_BAND, TEX_RING));
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      break;

    case TEX_CLOUDS:
      uiItemR(col, &tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "cloud_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
      row = uiLayoutRow(col, false);
      uiItemR(row, &tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
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
        uiItemR(col, &tex_ptr, "minkovsky_exponent", DEFAULT_FLAGS, nullptr, ICON_NONE);
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
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
}

static void node_texture_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  PointerRNA iuserptr;

  RNA_pointer_create(ptr->owner_id, &RNA_ImageUser, node->storage, &iuserptr);
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, false, false);
}

static void node_texture_buts_output(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filepath", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_texture_buts_combsep_color(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
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

      case TEX_NODE_COMBINE_COLOR:
      case TEX_NODE_SEPARATE_COLOR:
        ntype->draw_buttons = node_texture_buts_combsep_color;
        break;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Init Draw Callbacks For All Tree Types
 *
 * Only called on node initialization, once.
 * \{ */

static void node_property_update_default(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
  bNode *node = (bNode *)ptr->data;
  BKE_ntree_update_tag_node_property(ntree, node);
  ED_node_tree_propagate_change(nullptr, bmain, ntree);
}

static void node_socket_template_properties_update(bNodeType *ntype, bNodeSocketTemplate *stemp)
{
  StructRNA *srna = ntype->rna_ext.srna;
  PropertyRNA *prop = RNA_struct_type_find_property(srna, stemp->identifier);

  if (prop) {
    RNA_def_property_update_runtime(prop, (const void *)node_property_update_default);
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

static void node_socket_undefined_draw(bContext * /*C*/,
                                       uiLayout *layout,
                                       PointerRNA * /*ptr*/,
                                       PointerRNA * /*node_ptr*/,
                                       const char * /*text*/)
{
  uiItemL(layout, IFACE_("Undefined Socket Type"), ICON_ERROR);
}

static void node_socket_undefined_draw_color(bContext * /*C*/,
                                             PointerRNA * /*ptr*/,
                                             PointerRNA * /*node_ptr*/,
                                             float *r_color)
{
  r_color[0] = 1.0f;
  r_color[1] = 0.0f;
  r_color[2] = 0.0f;
  r_color[3] = 1.0f;
}

static void node_socket_undefined_interface_draw(bContext * /*C*/,
                                                 uiLayout *layout,
                                                 PointerRNA * /*ptr*/)
{
  uiItemL(layout, IFACE_("Undefined Socket Type"), ICON_ERROR);
}

static void node_socket_undefined_interface_draw_color(bContext * /*C*/,
                                                       PointerRNA * /*ptr*/,
                                                       float *r_color)
{
  r_color[0] = 1.0f;
  r_color[1] = 0.0f;
  r_color[2] = 0.0f;
  r_color[3] = 1.0f;
}

/** \} */

}  // namespace blender::ed::space_node

void ED_node_init_butfuncs()
{
  using namespace blender::ed::space_node;

  /* Fallback types for undefined tree, nodes, sockets
   * Defined in blenkernel, but not registered in type hashes.
   */

  using blender::bke::NodeSocketTypeUndefined;
  using blender::bke::NodeTypeUndefined;

  NodeTypeUndefined.draw_buttons = nullptr;
  NodeTypeUndefined.draw_buttons_ex = nullptr;

  NodeSocketTypeUndefined.draw = node_socket_undefined_draw;
  NodeSocketTypeUndefined.draw_color = node_socket_undefined_draw_color;
  NodeSocketTypeUndefined.interface_draw = node_socket_undefined_interface_draw;
  NodeSocketTypeUndefined.interface_draw_color = node_socket_undefined_interface_draw_color;

  /* node type ui functions */
  NODE_TYPES_BEGIN (ntype) {
    node_common_set_butfunc(ntype);

    node_composit_set_butfunc(ntype);
    node_shader_set_butfunc(ntype);
    node_texture_set_butfunc(ntype);

    /* define update callbacks for socket properties */
    node_template_properties_update(ntype);
  }
  NODE_TYPES_END;
}

void ED_init_custom_node_type(bNodeType * /*ntype*/) {}

void ED_init_custom_node_socket_type(bNodeSocketType *stype)
{
  stype->draw = blender::ed::space_node::node_socket_button_label;
}

namespace blender::ed::space_node {

static const float virtual_node_socket_color[4] = {0.2, 0.2, 0.2, 1.0};

/* maps standard socket integer type to a color */
static const float std_node_socket_colors[][4] = {
    {0.63, 0.63, 0.63, 1.0}, /* SOCK_FLOAT */
    {0.39, 0.39, 0.78, 1.0}, /* SOCK_VECTOR */
    {0.78, 0.78, 0.16, 1.0}, /* SOCK_RGBA */
    {0.39, 0.78, 0.39, 1.0}, /* SOCK_SHADER */
    {0.80, 0.65, 0.84, 1.0}, /* SOCK_BOOLEAN */
    {0.0, 0.0, 0.0, 1.0},    /*__SOCK_MESH (deprecated) */
    {0.35, 0.55, 0.36, 1.0}, /* SOCK_INT */
    {0.44, 0.70, 1.00, 1.0}, /* SOCK_STRING */
    {0.93, 0.62, 0.36, 1.0}, /* SOCK_OBJECT */
    {0.39, 0.22, 0.39, 1.0}, /* SOCK_IMAGE */
    {0.00, 0.84, 0.64, 1.0}, /* SOCK_GEOMETRY */
    {0.96, 0.96, 0.96, 1.0}, /* SOCK_COLLECTION */
    {0.62, 0.31, 0.64, 1.0}, /* SOCK_TEXTURE */
    {0.92, 0.46, 0.51, 1.0}, /* SOCK_MATERIAL */
};

/* common color callbacks for standard types */
static void std_node_socket_draw_color(bContext * /*C*/,
                                       PointerRNA *ptr,
                                       PointerRNA * /*node_ptr*/,
                                       float *r_color)
{
  bNodeSocket *sock = (bNodeSocket *)ptr->data;
  int type = sock->typeinfo->type;
  copy_v4_v4(r_color, std_node_socket_colors[type]);
}
static void std_node_socket_interface_draw_color(bContext * /*C*/, PointerRNA *ptr, float *r_color)
{
  bNodeSocket *sock = (bNodeSocket *)ptr->data;
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
  bNodeSocket *sock = (bNodeSocket *)ptr->data;
  uiLayout *row;
  PointerRNA inputptr;

  row = uiLayoutRow(layout, false);

  PointerRNA imfptr = RNA_pointer_get(node_ptr, "format");
  int imtype = RNA_enum_get(&imfptr, "file_format");

  if (imtype == R_IMF_IMTYPE_MULTILAYER) {
    NodeImageMultiFileSocket *input = (NodeImageMultiFileSocket *)sock->storage;
    RNA_pointer_create(&ntree->id, &RNA_NodeOutputFileSlotLayer, input, &inputptr);

    uiItemL(row, input->layer, ICON_NONE);
  }
  else {
    NodeImageMultiFileSocket *input = (NodeImageMultiFileSocket *)sock->storage;
    uiBlock *block;
    RNA_pointer_create(&ntree->id, &RNA_NodeOutputFileSlotFile, input, &inputptr);

    uiItemL(row, input->path, ICON_NONE);

    if (!RNA_boolean_get(&inputptr, "use_node_format")) {
      imfptr = RNA_pointer_get(&inputptr, "format");
    }

    const char *imtype_name;
    PropertyRNA *imtype_prop = RNA_struct_find_property(&imfptr, "file_format");
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

static bool socket_needs_attribute_search(bNode &node, bNodeSocket &socket)
{
  if (node.runtime->declaration == nullptr) {
    return false;
  }
  if (socket.in_out == SOCK_OUT) {
    return false;
  }
  const int socket_index = BLI_findindex(&node.inputs, &socket);
  return node.declaration()->inputs[socket_index]->is_attribute_name;
}

static void std_node_socket_draw(
    bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, const char *text)
{
  bNode *node = (bNode *)node_ptr->data;
  bNodeSocket *sock = (bNodeSocket *)ptr->data;
  int type = sock->typeinfo->type;
  // int subtype = sock->typeinfo->subtype;

  /* XXX not nice, eventually give this node its own socket type ... */
  if (node->type == CMP_NODE_OUTPUT_FILE) {
    node_file_output_socket_draw(C, layout, ptr, node_ptr);
    return;
  }

  if ((sock->in_out == SOCK_OUT) || (sock->flag & SOCK_IS_LINKED) ||
      (sock->flag & SOCK_HIDE_VALUE)) {
    node_socket_button_label(C, layout, ptr, node_ptr, text);
    return;
  }

  text = (sock->flag & SOCK_HIDE_LABEL) ? "" : text;

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
    case SOCK_RGBA: {
      if (text[0] == '\0') {
        uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, "", 0);
      }
      else {
        uiLayout *row = uiLayoutSplit(layout, 0.4f, false);
        uiItemL(row, text, 0);
        uiItemR(row, ptr, "default_value", DEFAULT_FLAGS, "", 0);
      }
      break;
    }
    case SOCK_STRING: {
      uiLayout *row = uiLayoutSplit(layout, 0.4f, false);
      uiItemL(row, text, 0);

      if (socket_needs_attribute_search(*node, *sock)) {
        node_geometry_add_attribute_search_button(*C, *node, *ptr, *row);
      }
      else {
        uiItemR(row, ptr, "default_value", DEFAULT_FLAGS, "", 0);
      }

      break;
    }
    case SOCK_OBJECT: {
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      break;
    }
    case SOCK_IMAGE: {
      const bNodeTree *node_tree = (const bNodeTree *)node_ptr->owner_id;
      if (node_tree->type == NTREE_GEOMETRY) {
        if (text[0] == '\0') {
          uiTemplateID(layout,
                       C,
                       ptr,
                       "default_value",
                       "image.new",
                       "image.open",
                       nullptr,
                       0,
                       ICON_NONE,
                       nullptr);
        }
        else {
          /* 0.3 split ratio is inconsistent, but use it here because the "New" button is large. */
          uiLayout *row = uiLayoutSplit(layout, 0.3f, false);
          uiItemL(row, text, 0);
          uiTemplateID(row,
                       C,
                       ptr,
                       "default_value",
                       "image.new",
                       "image.open",
                       nullptr,
                       0,
                       ICON_NONE,
                       nullptr);
        }
      }
      else {
        uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      }
      break;
    }
    case SOCK_COLLECTION: {
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      break;
    }
    case SOCK_TEXTURE: {
      if (text[0] == '\0') {
        uiTemplateID(layout,
                     C,
                     ptr,
                     "default_value",
                     "texture.new",
                     nullptr,
                     nullptr,
                     0,
                     ICON_NONE,
                     nullptr);
      }
      else {
        /* 0.3 split ratio is inconsistent, but use it here because the "New" button is large. */
        uiLayout *row = uiLayoutSplit(layout, 0.3f, false);
        uiItemL(row, text, 0);
        uiTemplateID(
            row, C, ptr, "default_value", "texture.new", nullptr, nullptr, 0, ICON_NONE, nullptr);
      }

      break;
    }
    case SOCK_MATERIAL: {
      uiItemR(layout, ptr, "default_value", DEFAULT_FLAGS, text, 0);
      break;
    }
    default:
      node_socket_button_label(C, layout, ptr, node_ptr, text);
      break;
  }
}

static void std_node_socket_interface_draw(bContext * /*C*/, uiLayout *layout, PointerRNA *ptr)
{
  bNodeSocket *sock = (bNodeSocket *)ptr->data;
  int type = sock->typeinfo->type;

  uiLayout *col = uiLayoutColumn(layout, false);

  switch (type) {
    case SOCK_FLOAT: {
      uiItemR(col, ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      uiLayout *sub = uiLayoutColumn(col, true);
      uiItemR(sub, ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
      uiItemR(sub, ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);
      break;
    }
    case SOCK_INT: {
      uiItemR(col, ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      uiLayout *sub = uiLayoutColumn(col, true);
      uiItemR(sub, ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
      uiItemR(sub, ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);
      break;
    }
    case SOCK_VECTOR: {
      uiItemR(col, ptr, "default_value", UI_ITEM_R_EXPAND, IFACE_("Default"), ICON_NONE);
      uiLayout *sub = uiLayoutColumn(col, true);
      uiItemR(sub, ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
      uiItemR(sub, ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);
      break;
    }
    case SOCK_BOOLEAN:
    case SOCK_RGBA:
    case SOCK_STRING:
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_IMAGE:
    case SOCK_TEXTURE:
    case SOCK_MATERIAL: {
      uiItemR(col, ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), 0);
      break;
    }
  }

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "hide_value", DEFAULT_FLAGS, nullptr, 0);

  const bNodeTree *node_tree = reinterpret_cast<const bNodeTree *>(ptr->owner_id);
  if (sock->in_out == SOCK_IN && node_tree->type == NTREE_GEOMETRY) {
    uiItemR(col, ptr, "hide_in_modifier", DEFAULT_FLAGS, nullptr, 0);
  }
}

static void node_socket_virtual_draw_color(bContext * /*C*/,
                                           PointerRNA * /*ptr*/,
                                           PointerRNA * /*node_ptr*/,
                                           float *r_color)
{
  copy_v4_v4(r_color, virtual_node_socket_color);
}

}  // namespace blender::ed::space_node

void ED_init_standard_node_socket_type(bNodeSocketType *stype)
{
  using namespace blender::ed::space_node;
  stype->draw = std_node_socket_draw;
  stype->draw_color = std_node_socket_draw_color;
  stype->interface_draw = std_node_socket_interface_draw;
  stype->interface_draw_color = std_node_socket_interface_draw_color;
}

void ED_init_node_socket_type_virtual(bNodeSocketType *stype)
{
  using namespace blender::ed::space_node;
  stype->draw = node_socket_button_label;
  stype->draw_color = node_socket_virtual_draw_color;
}

void ED_node_type_draw_color(const char *idname, float *r_color)
{
  using namespace blender::ed::space_node;

  const bNodeSocketType *typeinfo = nodeSocketTypeFind(idname);
  if (!typeinfo || typeinfo->type == SOCK_CUSTOM) {
    r_color[0] = 0.0f;
    r_color[1] = 0.0f;
    r_color[2] = 0.0f;
    r_color[3] = 0.0f;
    return;
  }

  BLI_assert(typeinfo->type < ARRAY_SIZE(std_node_socket_colors));
  copy_v4_v4(r_color, std_node_socket_colors[typeinfo->type]);
}

namespace blender::ed::space_node {

/* ************** Generic drawing ************** */

void draw_nodespace_back_pix(const bContext &C,
                             ARegion &region,
                             SpaceNode &snode,
                             bNodeInstanceKey parent_key)
{
  Main *bmain = CTX_data_main(&C);
  bNodeInstanceKey active_viewer_key = (snode.nodetree ? snode.nodetree->active_viewer_key :
                                                         NODE_INSTANCE_KEY_NONE);
  GPU_matrix_push_projection();
  GPU_matrix_push();
  wmOrtho2_region_pixelspace(&region);
  GPU_matrix_identity_set();
  ED_region_draw_cb_draw(&C, &region, REGION_DRAW_BACKDROP);
  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  if (!(snode.flag & SNODE_BACKDRAW) || !ED_node_is_compositor(&snode)) {
    return;
  }

  if (parent_key.value != active_viewer_key.value) {
    return;
  }

  GPU_matrix_push_projection();
  GPU_matrix_push();

  /* The draw manager is used to draw the backdrop image. */
  GPUFrameBuffer *old_fb = GPU_framebuffer_active_get();
  GPU_framebuffer_restore();
  BLI_thread_lock(LOCK_DRAW_IMAGE);
  DRW_draw_view(&C);
  BLI_thread_unlock(LOCK_DRAW_IMAGE);
  GPU_framebuffer_bind_no_srgb(old_fb);
  /* Draw manager changes the depth state. Set it back to NONE. Without this the node preview
   * images aren't drawn correctly. */
  GPU_depth_test(GPU_DEPTH_NONE);

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);
  if (ibuf) {
    /* somehow the offset has to be calculated inverse */
    wmOrtho2_region_pixelspace(&region);
    const float offset_x = snode.xof + ima->offset_x * snode.zoom;
    const float offset_y = snode.yof + ima->offset_y * snode.zoom;
    const float x = (region.winx - snode.zoom * ibuf->x) / 2 + offset_x;
    const float y = (region.winy - snode.zoom * ibuf->y) / 2 + offset_y;

    /** \note draw selected info on backdrop */
    if (snode.edittree) {
      bNode *node = (bNode *)snode.edittree->nodes.first;
      rctf *viewer_border = &snode.nodetree->viewer_border;
      while (node) {
        if (node->flag & NODE_SELECT) {
          if (node->typeinfo->draw_backdrop) {
            node->typeinfo->draw_backdrop(&snode, ibuf, node, x, y);
          }
        }
        node = node->next;
      }

      if ((snode.nodetree->flag & NTREE_VIEWER_BORDER) &&
          viewer_border->xmin < viewer_border->xmax && viewer_border->ymin < viewer_border->ymax)
      {
        rcti pixel_border;
        BLI_rcti_init(&pixel_border,
                      x + snode.zoom * viewer_border->xmin * ibuf->x,
                      x + snode.zoom * viewer_border->xmax * ibuf->x,
                      y + snode.zoom * viewer_border->ymin * ibuf->y,
                      y + snode.zoom * viewer_border->ymax * ibuf->y);

        uint pos = GPU_vertformat_attr_add(
            immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
        immUniformThemeColor(TH_ACTIVE);

        immDrawBorderCorners(pos, &pixel_border, 1.0f, 1.0f);

        immUnbindProgram();
      }
    }
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
  GPU_matrix_pop_projection();
  GPU_matrix_pop();
}

static float2 socket_link_connection_location(const bNode &node,
                                              const bNodeSocket &socket,
                                              const bNodeLink &link)
{
  const float2 socket_location = socket.runtime->location;
  if (socket.is_multi_input() && socket.is_input() && !(node.flag & NODE_HIDDEN)) {
    return node_link_calculate_multi_input_position(
        socket_location, link.multi_input_socket_index, socket.runtime->total_inputs);
  }
  return socket_location;
}

static void calculate_inner_link_bezier_points(std::array<float2, 4> &points)
{
  const int curving = UI_GetThemeValueType(TH_NODE_CURVING, SPACE_NODE);
  if (curving == 0) {
    /* Straight line: align all points. */
    points[1] = math::interpolate(points[0], points[3], 1.0f / 3.0f);
    points[2] = math::interpolate(points[0], points[3], 2.0f / 3.0f);
  }
  else {
    const float dist_x = math::distance(points[0].x, points[3].x);
    const float dist_y = math::distance(points[0].y, points[3].y);

    /* Reduce the handle offset when the link endpoints are close to horizontal. */
    const float slope = safe_divide(dist_y, dist_x);
    const float clamp_factor = math::min(1.0f, slope * (4.5f - 0.25f * float(curving)));

    const float handle_offset = curving * 0.1f * dist_x * clamp_factor;

    points[1].x = points[0].x + handle_offset;
    points[1].y = points[0].y;

    points[2].x = points[3].x - handle_offset;
    points[2].y = points[3].y;
  }
}

static std::array<float2, 4> node_link_bezier_points(const bNodeLink &link)
{
  std::array<float2, 4> points;
  points[0] = socket_link_connection_location(*link.fromnode, *link.fromsock, link);
  points[3] = socket_link_connection_location(*link.tonode, *link.tosock, link);
  calculate_inner_link_bezier_points(points);
  return points;
}

static bool node_link_draw_is_visible(const View2D &v2d, const std::array<float2, 4> &points)
{
  if (min_ffff(points[0].x, points[1].x, points[2].x, points[3].x) > v2d.cur.xmax) {
    return false;
  }
  if (max_ffff(points[0].x, points[1].x, points[2].x, points[3].x) < v2d.cur.xmin) {
    return false;
  }
  return true;
}

void node_link_bezier_points_evaluated(const bNodeLink &link,
                                       std::array<float2, NODE_LINK_RESOL + 1> &coords)
{
  const std::array<float2, 4> points = node_link_bezier_points(link);

  /* The extra +1 in size is required by these functions and would be removed ideally. */
  BKE_curve_forward_diff_bezier(points[0].x,
                                points[1].x,
                                points[2].x,
                                points[3].x,
                                &coords[0].x,
                                NODE_LINK_RESOL,
                                sizeof(float2));
  BKE_curve_forward_diff_bezier(points[0].y,
                                points[1].y,
                                points[2].y,
                                points[3].y,
                                &coords[0].y,
                                NODE_LINK_RESOL,
                                sizeof(float2));
}

#define NODELINK_GROUP_SIZE 256
#define LINK_RESOL 24
#define LINK_WIDTH (2.5f * UI_SCALE_FAC)
#define ARROW_SIZE (7 * UI_SCALE_FAC)

/* Reroute arrow shape and mute bar. These are expanded here and shrunk in the glsl code.
 * See: gpu_shader_2D_nodelink_vert.glsl */
static float arrow_verts[3][2] = {{-1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, -1.0f}};
static float arrow_expand_axis[3][2] = {{0.7071f, 0.7071f}, {M_SQRT2, 0.0f}, {0.7071f, -0.7071f}};
static float mute_verts[3][2] = {{0.7071f, 1.0f}, {0.7071f, 0.0f}, {0.7071f, -1.0f}};
static float mute_expand_axis[3][2] = {{1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, -0.0f}};

/* Is zero initialized because it is static data. */
static struct {
  GPUBatch *batch;        /* for batching line together */
  GPUBatch *batch_single; /* for single line */
  GPUVertBuf *inst_vbo;
  uint p0_id, p1_id, p2_id, p3_id;
  uint colid_id, muted_id, start_color_id, end_color_id;
  uint dim_factor_id;
  uint thickness_id;
  uint dash_factor_id;
  uint dash_alpha_id;
  GPUVertBufRaw p0_step, p1_step, p2_step, p3_step;
  GPUVertBufRaw colid_step, muted_step, start_color_step, end_color_step;
  GPUVertBufRaw dim_factor_step;
  GPUVertBufRaw thickness_step;
  GPUVertBufRaw dash_factor_step;
  GPUVertBufRaw dash_alpha_step;
  uint count;
  bool enabled;
} g_batch_link;

static void nodelink_batch_reset()
{
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p0_id, &g_batch_link.p0_step);
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p1_id, &g_batch_link.p1_step);
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p2_id, &g_batch_link.p2_step);
  GPU_vertbuf_attr_get_raw_data(g_batch_link.inst_vbo, g_batch_link.p3_id, &g_batch_link.p3_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.colid_id, &g_batch_link.colid_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.muted_id, &g_batch_link.muted_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.dim_factor_id, &g_batch_link.dim_factor_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.thickness_id, &g_batch_link.thickness_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.dash_factor_id, &g_batch_link.dash_factor_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.dash_alpha_id, &g_batch_link.dash_alpha_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.start_color_id, &g_batch_link.start_color_step);
  GPU_vertbuf_attr_get_raw_data(
      g_batch_link.inst_vbo, g_batch_link.end_color_id, &g_batch_link.end_color_step);
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

static void nodelink_batch_init()
{
  GPUVertFormat format = {0};
  uint uv_id = GPU_vertformat_attr_add(&format, "uv", GPU_COMP_U8, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
  uint pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint expand_id = GPU_vertformat_attr_add(&format, "expand", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_STATIC);
  int vcount = LINK_RESOL * 2; /* curve */
  vcount += 2;                 /* restart strip */
  vcount += 3 * 2;             /* arrow */
  vcount += 2;                 /* restart strip */
  vcount += 3 * 2;             /* mute */
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
      uv[0] = 255 * (i / float(LINK_RESOL - 1));
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
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

    uv[0] = 127;
    uv[1] = 0;
    copy_v2_v2(pos, mute_verts[0]);
    copy_v2_v2(exp, mute_expand_axis[0]);
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    /* bar */
    for (int i = 0; i < 3; ++i) {
      uv[1] = 0;
      copy_v2_v2(pos, mute_verts[i]);
      copy_v2_v2(exp, mute_expand_axis[i]);
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

      uv[1] = 255;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }

    /* restart */
    if (k == 0) {
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }
  }

  g_batch_link.batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  gpu_batch_presets_register(g_batch_link.batch);

  g_batch_link.batch_single = GPU_batch_create_ex(
      GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_INVALID);
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
  g_batch_link.start_color_id = GPU_vertformat_attr_add(
      &format_inst, "start_color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  g_batch_link.end_color_id = GPU_vertformat_attr_add(
      &format_inst, "end_color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  g_batch_link.muted_id = GPU_vertformat_attr_add(
      &format_inst, "domuted", GPU_COMP_U8, 2, GPU_FETCH_INT);
  g_batch_link.dim_factor_id = GPU_vertformat_attr_add(
      &format_inst, "dim_factor", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  g_batch_link.thickness_id = GPU_vertformat_attr_add(
      &format_inst, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  g_batch_link.dash_factor_id = GPU_vertformat_attr_add(
      &format_inst, "dash_factor", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  g_batch_link.dash_alpha_id = GPU_vertformat_attr_add(
      &format_inst, "dash_alpha", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
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

static void nodelink_batch_draw(const SpaceNode &snode)
{
  if (g_batch_link.count == 0) {
    return;
  }

  GPU_blend(GPU_BLEND_ALPHA);
  NodeLinkInstanceData node_link_data;

  UI_GetThemeColor4fv(TH_WIRE_INNER, node_link_data.colors[nodelink_get_color_id(TH_WIRE_INNER)]);
  UI_GetThemeColor4fv(TH_WIRE, node_link_data.colors[nodelink_get_color_id(TH_WIRE)]);
  UI_GetThemeColor4fv(TH_ACTIVE, node_link_data.colors[nodelink_get_color_id(TH_ACTIVE)]);
  UI_GetThemeColor4fv(TH_EDGE_SELECT,
                      node_link_data.colors[nodelink_get_color_id(TH_EDGE_SELECT)]);
  UI_GetThemeColor4fv(TH_REDALERT, node_link_data.colors[nodelink_get_color_id(TH_REDALERT)]);
  node_link_data.expandSize = snode.runtime->aspect * LINK_WIDTH;
  node_link_data.arrowSize = ARROW_SIZE;

  GPUUniformBuf *ubo = GPU_uniformbuf_create_ex(sizeof(node_link_data), &node_link_data, __func__);

  GPU_vertbuf_data_len_set(g_batch_link.inst_vbo, g_batch_link.count);
  GPU_vertbuf_use(g_batch_link.inst_vbo); /* force update. */

  GPU_batch_program_set_builtin(g_batch_link.batch, GPU_SHADER_2D_NODELINK_INST);
  GPU_batch_uniformbuf_bind(g_batch_link.batch, "node_link_data", ubo);
  GPU_batch_draw(g_batch_link.batch);

  GPU_uniformbuf_unbind(ubo);
  GPU_uniformbuf_free(ubo);

  nodelink_batch_reset();

  GPU_blend(GPU_BLEND_NONE);
}

void nodelink_batch_start(SpaceNode & /*snode*/)
{
  g_batch_link.enabled = true;
}

void nodelink_batch_end(SpaceNode &snode)
{
  nodelink_batch_draw(snode);
  g_batch_link.enabled = false;
}

struct NodeLinkDrawConfig {
  int th_col1;
  int th_col2;
  int th_col3;

  ColorTheme4f start_color;
  ColorTheme4f end_color;
  ColorTheme4f outline_color;

  bool drawarrow;
  bool drawmuted;
  bool highlighted;

  float dim_factor;
  float thickness;
  float dash_factor;
  float dash_alpha;
};

static void nodelink_batch_add_link(const SpaceNode &snode,
                                    const std::array<float2, 4> &points,
                                    const NodeLinkDrawConfig &draw_config)
{
  /* Only allow these colors. If more is needed, you need to modify the shader accordingly. */
  BLI_assert(
      ELEM(draw_config.th_col1, TH_WIRE_INNER, TH_WIRE, TH_ACTIVE, TH_EDGE_SELECT, TH_REDALERT));
  BLI_assert(
      ELEM(draw_config.th_col2, TH_WIRE_INNER, TH_WIRE, TH_ACTIVE, TH_EDGE_SELECT, TH_REDALERT));
  BLI_assert(ELEM(draw_config.th_col3, TH_WIRE, TH_REDALERT, -1));

  g_batch_link.count++;
  copy_v2_v2((float *)GPU_vertbuf_raw_step(&g_batch_link.p0_step), points[0]);
  copy_v2_v2((float *)GPU_vertbuf_raw_step(&g_batch_link.p1_step), points[1]);
  copy_v2_v2((float *)GPU_vertbuf_raw_step(&g_batch_link.p2_step), points[2]);
  copy_v2_v2((float *)GPU_vertbuf_raw_step(&g_batch_link.p3_step), points[3]);
  char *colid = (char *)GPU_vertbuf_raw_step(&g_batch_link.colid_step);
  colid[0] = nodelink_get_color_id(draw_config.th_col1);
  colid[1] = nodelink_get_color_id(draw_config.th_col2);
  colid[2] = nodelink_get_color_id(draw_config.th_col3);
  colid[3] = draw_config.drawarrow;
  copy_v4_v4((float *)GPU_vertbuf_raw_step(&g_batch_link.start_color_step),
             draw_config.start_color);
  copy_v4_v4((float *)GPU_vertbuf_raw_step(&g_batch_link.end_color_step), draw_config.end_color);
  char *muted = (char *)GPU_vertbuf_raw_step(&g_batch_link.muted_step);
  muted[0] = draw_config.drawmuted;
  *(float *)GPU_vertbuf_raw_step(&g_batch_link.dim_factor_step) = draw_config.dim_factor;
  *(float *)GPU_vertbuf_raw_step(&g_batch_link.thickness_step) = draw_config.thickness;
  *(float *)GPU_vertbuf_raw_step(&g_batch_link.dash_factor_step) = draw_config.dash_factor;
  *(float *)GPU_vertbuf_raw_step(&g_batch_link.dash_alpha_step) = draw_config.dash_alpha;

  if (g_batch_link.count == NODELINK_GROUP_SIZE) {
    nodelink_batch_draw(snode);
  }
}

static void node_draw_link_end_marker(const float2 center,
                                      const float radius,
                                      const ColorTheme4f &color)
{
  rctf rect;
  BLI_rctf_init(&rect, center.x - radius, center.x + radius, center.y - radius, center.y + radius);

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(&rect, true, radius, color);
  /* Round-box disables alpha. Re-enable it for node links that are drawn after this one. */
  GPU_blend(GPU_BLEND_ALPHA);
}

static void node_draw_link_end_markers(const bNodeLink &link,
                                       const NodeLinkDrawConfig &draw_config,
                                       const std::array<float2, 4> &points,
                                       const bool outline)
{
  const float radius = (outline ? 0.65f : 0.45f) * NODE_SOCKSIZE;
  if (link.fromsock) {
    node_draw_link_end_marker(
        points[0], radius, outline ? draw_config.outline_color : draw_config.start_color);
  }
  if (link.tosock) {
    node_draw_link_end_marker(
        points[3], radius, outline ? draw_config.outline_color : draw_config.end_color);
  }
}

static bool node_link_is_field_link(const SpaceNode &snode, const bNodeLink &link)
{
  if (snode.edittree->type != NTREE_GEOMETRY) {
    return false;
  }
  if (link.fromsock && link.fromsock->display_shape == SOCK_DISPLAY_SHAPE_DIAMOND) {
    return true;
  }
  return false;
}

static NodeLinkDrawConfig nodelink_get_draw_config(const bContext &C,
                                                   const View2D &v2d,
                                                   const SpaceNode &snode,
                                                   const bNodeLink &link,
                                                   const int th_col1,
                                                   const int th_col2,
                                                   const int th_col3,
                                                   const bool selected)
{
  NodeLinkDrawConfig draw_config;

  draw_config.th_col1 = th_col1;
  draw_config.th_col2 = th_col2;
  draw_config.th_col3 = th_col3;

  const bNodeTree &node_tree = *snode.edittree;

  draw_config.dim_factor = selected ? 1.0f : node_link_dim_factor(v2d, link);

  bTheme *btheme = UI_GetTheme();
  draw_config.dash_alpha = btheme->space_node.dash_alpha;

  const bool field_link = node_link_is_field_link(snode, link);

  draw_config.dash_factor = field_link ? 0.75f : 1.0f;

  const float scale = UI_view2d_scale_get_x(&v2d);
  /* Clamp the thickness to make the links more readable when zooming out. */
  draw_config.thickness = max_ff(scale, 1.0f) * (field_link ? 0.7f : 1.0f);
  draw_config.highlighted = link.flag & NODE_LINK_TEMP_HIGHLIGHT;
  draw_config.drawarrow = ((link.tonode && (link.tonode->type == NODE_REROUTE)) &&
                           (link.fromnode && (link.fromnode->type == NODE_REROUTE)));
  draw_config.drawmuted = (link.flag & NODE_LINK_MUTED);

  UI_GetThemeColor4fv(th_col3, draw_config.outline_color);

  if (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS &&
      snode.overlay.flag & SN_OVERLAY_SHOW_WIRE_COLORS)
  {
    PointerRNA from_node_ptr, to_node_ptr;
    RNA_pointer_create((ID *)&node_tree, &RNA_Node, link.fromnode, &from_node_ptr);
    RNA_pointer_create((ID *)&node_tree, &RNA_Node, link.tonode, &to_node_ptr);

    if (link.fromsock) {
      node_socket_color_get(C, node_tree, from_node_ptr, *link.fromsock, draw_config.start_color);
    }
    else {
      node_socket_color_get(C, node_tree, to_node_ptr, *link.tosock, draw_config.start_color);
    }

    if (link.tosock) {
      node_socket_color_get(C, node_tree, to_node_ptr, *link.tosock, draw_config.end_color);
    }
    else {
      node_socket_color_get(C, node_tree, from_node_ptr, *link.fromsock, draw_config.end_color);
    }
  }
  else {
    UI_GetThemeColor4fv(th_col1, draw_config.start_color);
    UI_GetThemeColor4fv(th_col2, draw_config.end_color);
  }

  /* Highlight links connected to selected nodes. */
  if (selected) {
    ColorTheme4f color_selected;
    UI_GetThemeColor4fv(TH_EDGE_SELECT, color_selected);
    const float alpha = color_selected.a;

    /* Interpolate color if highlight color is not fully transparent. */
    if (alpha != 0.0) {
      if (link.fromsock) {
        interp_v3_v3v3(draw_config.start_color, draw_config.start_color, color_selected, alpha);
      }
      if (link.tosock) {
        interp_v3_v3v3(draw_config.end_color, draw_config.end_color, color_selected, alpha);
      }
    }
  }

  if (draw_config.highlighted) {
    ColorTheme4f link_preselection_highlight_color;
    UI_GetThemeColor4fv(TH_SELECT, link_preselection_highlight_color);
    /* Multi sockets can only be inputs. So we only have to highlight the end of the link. */
    copy_v4_v4(draw_config.end_color, link_preselection_highlight_color);
  }

  return draw_config;
}

static void node_draw_link_bezier_ex(const SpaceNode &snode,
                                     const NodeLinkDrawConfig &draw_config,
                                     const std::array<float2, 4> &points)
{
  if (g_batch_link.batch == nullptr) {
    nodelink_batch_init();
  }

  if (g_batch_link.enabled && !draw_config.highlighted) {
    /* Add link to batch. */
    nodelink_batch_add_link(snode, points, draw_config);
  }
  else {
    NodeLinkData node_link_data;
    for (const int i : IndexRange(points.size())) {
      copy_v2_v2(node_link_data.bezierPts[i], points[i]);
    }

    copy_v4_v4(node_link_data.colors[0], draw_config.outline_color);
    copy_v4_v4(node_link_data.colors[1], draw_config.start_color);
    copy_v4_v4(node_link_data.colors[2], draw_config.end_color);

    node_link_data.doArrow = draw_config.drawarrow;
    node_link_data.doMuted = draw_config.drawmuted;
    node_link_data.dim_factor = draw_config.dim_factor;
    node_link_data.thickness = draw_config.thickness;
    node_link_data.dash_factor = draw_config.dash_factor;
    node_link_data.dash_alpha = draw_config.dash_alpha;
    node_link_data.expandSize = snode.runtime->aspect * LINK_WIDTH;
    node_link_data.arrowSize = ARROW_SIZE;

    GPUBatch *batch = g_batch_link.batch_single;
    GPUUniformBuf *ubo = GPU_uniformbuf_create_ex(sizeof(NodeLinkData), &node_link_data, __func__);

    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_NODELINK);
    GPU_batch_uniformbuf_bind(batch, "node_link_data", ubo);
    GPU_batch_draw(batch);

    GPU_uniformbuf_unbind(ubo);
    GPU_uniformbuf_free(ubo);
  }
}

void node_draw_link_bezier(const bContext &C,
                           const View2D &v2d,
                           const SpaceNode &snode,
                           const bNodeLink &link,
                           const int th_col1,
                           const int th_col2,
                           const int th_col3,
                           const bool selected)
{
  const std::array<float2, 4> points = node_link_bezier_points(link);
  if (!node_link_draw_is_visible(v2d, points)) {
    return;
  }
  const NodeLinkDrawConfig draw_config = nodelink_get_draw_config(
      C, v2d, snode, link, th_col1, th_col2, th_col3, selected);

  node_draw_link_bezier_ex(snode, draw_config, points);
}

void node_draw_link(const bContext &C,
                    const View2D &v2d,
                    const SpaceNode &snode,
                    const bNodeLink &link,
                    const bool selected)
{
  int th_col1 = TH_WIRE_INNER, th_col2 = TH_WIRE_INNER, th_col3 = TH_WIRE;

  if (link.fromsock == nullptr && link.tosock == nullptr) {
    return;
  }

  /* going to give issues once... */
  if (link.tosock->flag & SOCK_UNAVAIL) {
    return;
  }
  if (link.fromsock->flag & SOCK_UNAVAIL) {
    return;
  }

  if (link.flag & NODE_LINK_VALID) {
    /* special indicated link, on drop-node */
    if (link.flag & NODE_LINKFLAG_HILITE) {
      th_col1 = th_col2 = TH_ACTIVE;
    }
    else if (link.flag & NODE_LINK_MUTED) {
      th_col1 = th_col2 = TH_REDALERT;
    }
  }
  else {
    /* Invalid link. */
    th_col1 = th_col2 = th_col3 = TH_REDALERT;
    // th_col3 = -1; /* no shadow */
  }

  /* Links from field to non-field sockets are not allowed. */
  if (snode.edittree->type == NTREE_GEOMETRY) {
    if ((link.fromsock && link.fromsock->display_shape == SOCK_DISPLAY_SHAPE_DIAMOND) &&
        (link.tosock && link.tosock->display_shape == SOCK_DISPLAY_SHAPE_CIRCLE))
    {
      th_col1 = th_col2 = th_col3 = TH_REDALERT;
    }
  }

  node_draw_link_bezier(C, v2d, snode, link, th_col1, th_col2, th_col3, selected);
}

std::array<float2, 4> node_link_bezier_points_dragged(const SpaceNode &snode,
                                                      const bNodeLink &link)
{
  const float2 cursor = snode.runtime->cursor * UI_SCALE_FAC;
  std::array<float2, 4> points;
  points[0] = link.fromsock ?
                  socket_link_connection_location(*link.fromnode, *link.fromsock, link) :
                  cursor;
  points[3] = link.tosock ? socket_link_connection_location(*link.tonode, *link.tosock, link) :
                            cursor;
  calculate_inner_link_bezier_points(points);
  return points;
}

void node_draw_link_dragged(const bContext &C,
                            const View2D &v2d,
                            const SpaceNode &snode,
                            const bNodeLink &link)
{
  if (link.fromsock == nullptr && link.tosock == nullptr) {
    return;
  }

  const std::array<float2, 4> points = node_link_bezier_points_dragged(snode, link);

  const NodeLinkDrawConfig draw_config = nodelink_get_draw_config(
      C, v2d, snode, link, TH_ACTIVE, TH_ACTIVE, TH_WIRE, true);
  /* End marker outline. */
  node_draw_link_end_markers(link, draw_config, points, true);
  /* Link. */
  node_draw_link_bezier_ex(snode, draw_config, points);
  /* End marker fill. */
  node_draw_link_end_markers(link, draw_config, points, false);
}

}  // namespace blender::ed::space_node

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
