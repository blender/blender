/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 * \brief lower level node drawing for nodes (boarders, headers etc), also node layout.
 */

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"

#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_scene.hh"
#include "BKE_tracking.h"

#include "BLT_translation.hh"

#include "BIF_glutil.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_uniform_buffer.hh"

#include "DRW_engine.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "ED_node.hh"
#include "ED_space_api.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "NOD_composite.hh"
#include "NOD_geometry.hh"
#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_node_declaration.hh"
#include "NOD_partial_eval.hh"
#include "NOD_socket.hh"
#include "NOD_socket_declarations.hh"
#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

/* Default flags for uiLayout::prop(). Name is kept short since this is used a lot in this file. */
#define DEFAULT_FLAGS UI_ITEM_R_SPLIT_EMPTY_NAME

/* ****************** SOCKET BUTTON DRAW FUNCTIONS ***************** */

static void node_socket_button_label(bContext * /*C*/,
                                     uiLayout *layout,
                                     PointerRNA * /*ptr*/,
                                     PointerRNA * /*node_ptr*/,
                                     const StringRef text)
{
  layout->label(text, ICON_NONE);
}

/* ****************** BUTTON CALLBACKS FOR ALL TREES ***************** */

static void node_buts_mix_rgb(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNodeTree *ntree = (bNodeTree *)ptr->owner_id;

  uiLayout *col = &layout->column(false);
  uiLayout *row = &col->row(true);
  row->prop(ptr, "blend_type", DEFAULT_FLAGS, "", ICON_NONE);
  if (ELEM(ntree->type, NTREE_COMPOSIT, NTREE_TEXTURE)) {
    row->prop(ptr, "use_alpha", DEFAULT_FLAGS, "", ICON_IMAGE_RGB_ALPHA);
  }

  col->prop(ptr, "use_clamp", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
}

static void node_buts_time(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "curve", 's', false, false, false, false, false);
}

static void node_buts_colorramp(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateColorRamp(layout, ptr, "color_ramp", false);
}

static void node_buts_curvevec(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 'v', false, false, false, false, false);
}

static void node_buts_curvefloat(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 0, false, false, false, false, false);
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
      layout, ptr, "mapping", 'c', false, false, false, (ntree->type == NTREE_COMPOSIT), false);
}

static void node_buts_normal(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  /* first output stores normal */
  bNodeSocket *output = (bNodeSocket *)node->outputs.first;
  PointerRNA sockptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_NodeSocket, output);

  layout->prop(&sockptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_texture(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  short multi = (node->id && ((Tex *)node->id)->use_nodes &&
                 (node->type_legacy != TEX_NODE_TEXTURE));

  uiTemplateID(layout, C, ptr, "texture", "texture.new", nullptr, nullptr);

  if (multi) {
    /* Number Drawing not optimal here, better have a list. */
    layout->prop(ptr, "node_output", DEFAULT_FLAGS, "", ICON_NONE);
  }
}

static void node_buts_math(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "operation", DEFAULT_FLAGS, "", ICON_NONE);
  layout->prop(ptr, "use_clamp", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
}

static void node_buts_combsep_color(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

NodeResizeDirection node_get_resize_direction(const SpaceNode &snode,
                                              const bNode *node,
                                              const int x,
                                              const int y)
{
  const bool node_is_collapsed = node->flag & NODE_COLLAPSED;
  const float size = NODE_RESIZE_MARGIN * math::max(snode.runtime->aspect, 1.0f) *
                     (node_is_collapsed ? 3.0f : 1.0f);

  if (node->is_frame()) {
    NodeFrame *data = (NodeFrame *)node->storage;

    /* shrinking frame size is determined by child nodes */
    if (!(data->flag & NODE_FRAME_RESIZEABLE)) {
      return NODE_RESIZE_NONE;
    }

    NodeResizeDirection dir = NODE_RESIZE_NONE;

    const rctf &bounds = node->runtime->draw_bounds;

    if (x > bounds.xmax - size && x <= bounds.xmax && y >= bounds.ymin && y < bounds.ymax) {
      dir |= NODE_RESIZE_RIGHT;
    }
    if (x >= bounds.xmin && x < bounds.xmin + size && y >= bounds.ymin && y < bounds.ymax) {
      dir |= NODE_RESIZE_LEFT;
    }
    if (x >= bounds.xmin && x < bounds.xmax && y >= bounds.ymax - size && y < bounds.ymax) {
      dir |= NODE_RESIZE_TOP;
    }
    if (x >= bounds.xmin && x < bounds.xmax && y >= bounds.ymin && y < bounds.ymin + size) {
      dir |= NODE_RESIZE_BOTTOM;
    }

    return dir;
  }

  const rctf &bounds = node->runtime->draw_bounds;
  NodeResizeDirection dir = NODE_RESIZE_NONE;

  if (x >= bounds.xmax - size && x < bounds.xmax && y >= bounds.ymin && y < bounds.ymax) {
    dir |= NODE_RESIZE_RIGHT;
  }
  if (x >= bounds.xmin && x < bounds.xmin + size && y >= bounds.ymin && y < bounds.ymax) {
    dir |= NODE_RESIZE_LEFT;

    if (node_is_collapsed) {
      /* Prevent conflict with the collapse/expand icon. */
      if ((abs(y - BLI_rctf_cent_y(&bounds)) < 0.4f * U.widget_unit) &&
          (x > (bounds.xmin + 0.4f * U.widget_unit)))
      {
        dir = NODE_RESIZE_NONE;
      }
    }
  }
  return dir;
}

/* ****************** BUTTON CALLBACKS FOR COMMON NODES ***************** */

static void node_draw_buttons_group(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateIDBrowse(layout, C, ptr, "node_tree", nullptr, nullptr, nullptr);
}

static void node_buts_frame_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "label_size", DEFAULT_FLAGS, IFACE_("Label Size"), ICON_NONE);
  layout->prop(ptr, "shrink", DEFAULT_FLAGS, IFACE_("Shrink"), ICON_NONE);
  layout->prop(ptr, "text", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
}

static void node_common_set_butfunc(blender::bke::bNodeType *ntype)
{
  switch (ntype->type_legacy) {
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

  uiLayout *col = &layout->column(false);

  col->prop(imaptr, "source", DEFAULT_FLAGS, "", ICON_NONE);

  const int source = RNA_enum_get(imaptr, "source");

  if (source == IMA_SRC_SEQUENCE) {
    /* don't use iuser->framenr directly
     * because it may not be updated if auto-refresh is off */
    Scene *scene = CTX_data_scene(C);

    char numstr[32];
    const int framenr = BKE_image_user_frame_get(iuser, scene->r.cfra, nullptr);
    SNPRINTF_UTF8(numstr, IFACE_("Frame: %d"), framenr);
    layout->label(numstr, ICON_NONE);
  }

  if (ELEM(source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    col = &layout->column(true);
    col->prop(ptr, "frame_duration", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    col->prop(ptr, "frame_start", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    col->prop(ptr, "frame_offset", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    col->prop(ptr, "use_cyclic", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    col->prop(ptr, "use_auto_refresh", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
  }

  if (show_layer_selection && RNA_enum_get(imaptr, "type") == IMA_TYPE_MULTILAYER &&
      RNA_boolean_get(ptr, "has_layers"))
  {
    col = &layout->column(false);
    col->prop(ptr, "layer", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
  }

  if (show_color_management) {
    uiLayout *split = &layout->split(0.33f, true);
    PointerRNA colorspace_settings_ptr = RNA_pointer_get(imaptr, "colorspace_settings");
    split->label(IFACE_("Color Space"), ICON_NONE);
    split->prop(&colorspace_settings_ptr, "name", DEFAULT_FLAGS, "", ICON_NONE);

    if (image->source != IMA_SRC_GENERATED) {
      split = &layout->split(0.33f, true);
      split->label(IFACE_("Alpha"), ICON_NONE);
      split->prop(imaptr, "alpha_mode", DEFAULT_FLAGS, "", ICON_NONE);

      bool is_data = IMB_colormanagement_space_name_is_data(image->colorspace_settings.name);
      split->active_set(!is_data);
    }

    /* Avoid losing changes image is painted. */
    if (BKE_image_is_dirty((Image *)imaptr->data)) {
      split->enabled_set(false);
    }
  }
}

static void node_shader_buts_tex_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA imaptr = RNA_pointer_get(ptr, "image");
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");

  layout->context_ptr_set("image_user", &iuserptr);
  uiTemplateID(layout, C, ptr, "image", "IMAGE_OT_new", "IMAGE_OT_open", nullptr);
  layout->prop(ptr, "interpolation", DEFAULT_FLAGS, "", ICON_NONE);
  layout->prop(ptr, "projection", DEFAULT_FLAGS, "", ICON_NONE);

  if (RNA_enum_get(ptr, "projection") == SHD_PROJ_BOX) {
    layout->prop(ptr, "projection_blend", DEFAULT_FLAGS, IFACE_("Blend"), ICON_NONE);
  }

  layout->prop(ptr, "extension", DEFAULT_FLAGS, "", ICON_NONE);

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

  layout->context_ptr_set("image_user", &iuserptr);
  uiTemplateID(layout, C, ptr, "image", "IMAGE_OT_new", "IMAGE_OT_open", nullptr);

  layout->prop(ptr, "interpolation", DEFAULT_FLAGS, "", ICON_NONE);
  layout->prop(ptr, "projection", DEFAULT_FLAGS, "", ICON_NONE);

  node_buts_image_user(layout, C, &iuserptr, &imaptr, &iuserptr, false, true);
}

static void node_shader_buts_tex_environment_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA iuserptr = RNA_pointer_get(ptr, "image_user");
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, false, false);

  layout->prop(ptr, "interpolation", DEFAULT_FLAGS, IFACE_("Interpolation"), ICON_NONE);
  layout->prop(ptr, "projection", DEFAULT_FLAGS, IFACE_("Projection"), ICON_NONE);
}

static void node_shader_buts_displacement(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "space", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_glossy(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "distribution", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_buts_output_shader(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "target", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_shader_buts_scatter(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "phase", DEFAULT_FLAGS, "", ICON_NONE);
}

/* only once called */
static void node_shader_set_butfunc(blender::bke::bNodeType *ntype)
{
  switch (ntype->type_legacy) {
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
    case SH_NODE_BSDF_REFRACTION:
      ntype->draw_buttons = node_shader_buts_glossy;
      break;
    case SH_NODE_OUTPUT_MATERIAL:
    case SH_NODE_OUTPUT_LIGHT:
    case SH_NODE_OUTPUT_WORLD:
      ntype->draw_buttons = node_buts_output_shader;
      break;
    case SH_NODE_VOLUME_SCATTER:
      ntype->draw_buttons = node_shader_buts_scatter;
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

  col = &layout->column(false);

  if (RNA_boolean_get(ptr, "has_views")) {
    if (RNA_enum_get(ptr, "view") == 0) {
      col->prop(ptr, "view", DEFAULT_FLAGS, std::nullopt, ICON_CAMERA_STEREO);
    }
    else {
      col->prop(ptr, "view", DEFAULT_FLAGS, std::nullopt, ICON_SCENE);
    }
  }
}

static void node_composit_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  PointerRNA iuserptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_ImageUser, node->storage);
  layout->context_ptr_set("image_user", &iuserptr);
  uiTemplateID(layout, C, ptr, "image", "IMAGE_OT_new", "IMAGE_OT_open", nullptr);
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

  PointerRNA iuserptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_ImageUser, node->storage);
  layout->context_ptr_set("image_user", &iuserptr);
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

  uiTemplateCurveMapping(layout, ptr, "mapping", 'h', false, false, false, false, false);
}

static void node_composit_buts_combsep_color(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;

  layout->prop(ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
  if (storage->mode == CMP_NODE_COMBSEP_COLOR_YCC) {
    layout->prop(ptr, "ycc_mode", DEFAULT_FLAGS, "", ICON_NONE);
  }
}

static void node_composit_buts_cryptomatte_legacy(uiLayout *layout,
                                                  bContext * /*C*/,
                                                  PointerRNA *ptr)
{
  uiLayout *col = &layout->column(true);

  col->label(IFACE_("Matte Objects:"), ICON_NONE);

  uiLayout *row = &col->row(true);
  uiTemplateCryptoPicker(row, ptr, "add", ICON_ADD);
  uiTemplateCryptoPicker(row, ptr, "remove", ICON_REMOVE);

  col->prop(ptr, "matte_id", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_composit_buts_cryptomatte_legacy_ex(uiLayout *layout,
                                                     bContext * /*C*/,
                                                     PointerRNA * /*ptr*/)
{
  layout->op("NODE_OT_cryptomatte_layer_add", IFACE_("Add Crypto Layer"), ICON_ADD);
  layout->op("NODE_OT_cryptomatte_layer_remove", IFACE_("Remove Crypto Layer"), ICON_REMOVE);
}

static void node_composit_buts_cryptomatte(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiLayout *row = &layout->row(true);
  row->prop(ptr, "source", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  uiLayout *col = &layout->column(false);
  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    uiTemplateID(col, C, ptr, "scene", nullptr, nullptr, nullptr);
  }
  else {
    uiTemplateID(col, C, ptr, "image", nullptr, "IMAGE_OT_open", nullptr);

    NodeCryptomatte *crypto = (NodeCryptomatte *)node->storage;
    PointerRNA imaptr = RNA_pointer_get(ptr, "image");
    PointerRNA iuserptr = RNA_pointer_create_discrete(
        ptr->owner_id, &RNA_ImageUser, &crypto->iuser);
    layout->context_ptr_set("image_user", &iuserptr);

    node_buts_image_user(col, C, ptr, &imaptr, &iuserptr, false, false);
    node_buts_image_views(col, C, ptr, &imaptr);
  }

  col = &layout->column(true);
  col->prop(ptr, "layer_name", UI_ITEM_NONE, "", ICON_NONE);
  col->label(IFACE_("Matte ID:"), ICON_NONE);

  row = &col->row(true);
  row->prop(ptr, "matte_id", DEFAULT_FLAGS, "", ICON_NONE);
  uiTemplateCryptoPicker(row, ptr, "add", ICON_ADD);
  uiTemplateCryptoPicker(row, ptr, "remove", ICON_REMOVE);
}

/* only once called */
static void node_composit_set_butfunc(blender::bke::bNodeType *ntype)
{
  switch (ntype->type_legacy) {
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
    case CMP_NODE_TIME:
      ntype->draw_buttons = node_buts_time;
      break;
    case CMP_NODE_HUECORRECT:
      ntype->draw_buttons = node_composit_buts_huecorrect;
      break;
    case CMP_NODE_COMBINE_COLOR:
    case CMP_NODE_SEPARATE_COLOR:
      ntype->draw_buttons = node_composit_buts_combsep_color;
      break;
    case CMP_NODE_CRYPTOMATTE:
      ntype->draw_buttons = node_composit_buts_cryptomatte;
      break;
    case CMP_NODE_CRYPTOMATTE_LEGACY:
      ntype->draw_buttons = node_composit_buts_cryptomatte_legacy;
      ntype->draw_buttons_ex = node_composit_buts_cryptomatte_legacy_ex;
      break;
  }
}

/* ****************** BUTTON CALLBACKS FOR TEXTURE NODES ***************** */

static void node_texture_buts_bricks(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = &layout->column(true);
  col->prop(ptr, "offset", DEFAULT_FLAGS | UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
  col->prop(ptr, "offset_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);

  col = &layout->column(true);
  col->prop(ptr, "squash", DEFAULT_FLAGS, IFACE_("Squash"), ICON_NONE);
  col->prop(ptr, "squash_frequency", DEFAULT_FLAGS, IFACE_("Frequency"), ICON_NONE);
}

static void node_texture_buts_proc(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  ID *id = ptr->owner_id;
  Tex *tex = (Tex *)node->storage;
  uiLayout *col, *row;

  PointerRNA tex_ptr = RNA_pointer_create_discrete(id, &RNA_Texture, tex);

  col = &layout->column(false);

  switch (tex->type) {
    case TEX_BLEND:
      col->prop(&tex_ptr, "progression", DEFAULT_FLAGS, "", ICON_NONE);
      row = &col->row(false);
      row->prop(
          &tex_ptr, "use_flip_axis", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      break;

    case TEX_MARBLE:
      row = &col->row(false);
      row->prop(
          &tex_ptr, "marble_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      row = &col->row(false);
      row->prop(&tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      row = &col->row(false);
      row->prop(&tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      row = &col->row(false);
      row->prop(
          &tex_ptr, "noise_basis_2", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      break;

    case TEX_MAGIC:
      col->prop(&tex_ptr, "noise_depth", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
      break;

    case TEX_STUCCI:
      row = &col->row(false);
      row->prop(
          &tex_ptr, "stucci_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      row = &col->row(false);
      row->prop(&tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      col->prop(&tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      break;

    case TEX_WOOD:
      col->prop(&tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      col->prop(&tex_ptr, "wood_type", DEFAULT_FLAGS, "", ICON_NONE);
      row = &col->row(false);
      row->prop(
          &tex_ptr, "noise_basis_2", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      row = &col->row(false);
      row->active_set(!ELEM(tex->stype, TEX_BAND, TEX_RING));
      row->prop(&tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      break;

    case TEX_CLOUDS:
      col->prop(&tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      row = &col->row(false);
      row->prop(&tex_ptr, "cloud_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      row = &col->row(false);
      row->prop(&tex_ptr, "noise_type", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      col->prop(
          &tex_ptr, "noise_depth", DEFAULT_FLAGS | UI_ITEM_R_EXPAND, IFACE_("Depth"), ICON_NONE);
      break;

    case TEX_DISTNOISE:
      col->prop(&tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      col->prop(&tex_ptr, "noise_distortion", DEFAULT_FLAGS, "", ICON_NONE);
      break;

    case TEX_MUSGRAVE:
      col->prop(&tex_ptr, "musgrave_type", DEFAULT_FLAGS, "", ICON_NONE);
      col->prop(&tex_ptr, "noise_basis", DEFAULT_FLAGS, "", ICON_NONE);
      break;
    case TEX_VORONOI:
      col->prop(&tex_ptr, "distance_metric", DEFAULT_FLAGS, "", ICON_NONE);
      if (tex->vn_distm == TEX_MINKOVSKY) {
        col->prop(&tex_ptr, "minkovsky_exponent", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
      }
      col->prop(&tex_ptr, "color_mode", DEFAULT_FLAGS, "", ICON_NONE);
      break;
  }
}

static void node_texture_buts_image(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(layout, C, ptr, "image", "IMAGE_OT_new", "IMAGE_OT_open", nullptr);
}

static void node_texture_buts_image_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  PointerRNA iuserptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_ImageUser, node->storage);
  uiTemplateImage(layout, C, ptr, "image", &iuserptr, false, false);
}

static void node_texture_buts_output(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "filepath", DEFAULT_FLAGS, "", ICON_NONE);
}

static void node_texture_buts_combsep_color(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", DEFAULT_FLAGS, "", ICON_NONE);
}

/* only once called */
static void node_texture_set_butfunc(blender::bke::bNodeType *ntype)
{
  if (ntype->type_legacy >= TEX_NODE_PROC && ntype->type_legacy < TEX_NODE_PROC_MAX) {
    ntype->draw_buttons = node_texture_buts_proc;
  }
  else {
    switch (ntype->type_legacy) {

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
  BKE_main_ensure_invariants(*bmain);
}

static void node_socket_template_properties_update(blender::bke::bNodeType *ntype,
                                                   blender::bke::bNodeSocketTemplate *stemp)
{
  StructRNA *srna = ntype->rna_ext.srna;
  PropertyRNA *prop = RNA_struct_type_find_property(srna, stemp->identifier);

  if (prop) {
    RNA_def_property_update_runtime(prop, node_property_update_default);
  }
}

static void node_template_properties_update(blender::bke::bNodeType *ntype)
{
  blender::bke::bNodeSocketTemplate *stemp;

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
                                       StringRef /*text*/)
{
  layout->label(IFACE_("Undefined Socket Type"), ICON_ERROR);
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

static void node_socket_undefined_draw_color_simple(const bke::bNodeSocketType * /*type*/,
                                                    float *r_color)
{
  r_color[0] = 1.0f;
  r_color[1] = 0.0f;
  r_color[2] = 0.0f;
  r_color[3] = 1.0f;
}

static void node_socket_undefined_interface_draw(ID * /*id*/,
                                                 bNodeTreeInterfaceSocket * /*interface_socket*/,
                                                 bContext * /*C*/,
                                                 uiLayout *layout)
{
  layout->label(IFACE_("Undefined Socket Type"), ICON_ERROR);
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
  NodeSocketTypeUndefined.draw_color_simple = node_socket_undefined_draw_color_simple;
  NodeSocketTypeUndefined.interface_draw = node_socket_undefined_interface_draw;

  /* node type ui functions */
  for (blender::bke::bNodeType *ntype : blender::bke::node_types_get()) {
    node_common_set_butfunc(ntype);

    node_composit_set_butfunc(ntype);
    node_shader_set_butfunc(ntype);
    node_texture_set_butfunc(ntype);

    /* define update callbacks for socket properties */
    node_template_properties_update(ntype);
  }
}

void ED_init_custom_node_type(blender::bke::bNodeType * /*ntype*/) {}

void ED_init_custom_node_socket_type(blender::bke::bNodeSocketType *stype)
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
    {0.0, 0.0, 0.0, 0.0},    /* UNUSED */
    {0.35, 0.55, 0.36, 1.0}, /* SOCK_INT */
    {0.44, 0.70, 1.00, 1.0}, /* SOCK_STRING */
    {0.93, 0.62, 0.36, 1.0}, /* SOCK_OBJECT */
    {0.39, 0.22, 0.39, 1.0}, /* SOCK_IMAGE */
    {0.00, 0.84, 0.64, 1.0}, /* SOCK_GEOMETRY */
    {0.96, 0.96, 0.96, 1.0}, /* SOCK_COLLECTION */
    {0.62, 0.31, 0.64, 1.0}, /* SOCK_TEXTURE */
    {0.92, 0.46, 0.51, 1.0}, /* SOCK_MATERIAL */
    {0.65, 0.39, 0.78, 1.0}, /* SOCK_ROTATION */
    {0.40, 0.40, 0.40, 1.0}, /* SOCK_MENU */
    {0.72, 0.20, 0.52, 1.0}, /* SOCK_MATRIX */
    {0.30, 0.50, 0.50, 1.0}, /* SOCK_BUNDLE */
    {0.49, 0.49, 0.23, 1.0}, /* SOCK_CLOSURE */
};

void std_node_socket_colors_get(int socket_type, float *r_color)
{
  BLI_assert(socket_type >= 0);
  BLI_assert(socket_type < std::size(std_node_socket_colors));

  copy_v4_v4(r_color, std_node_socket_colors[socket_type]);
}

/* Callback for colors that does not depend on the socket pointer argument to get the type. */
template<int socket_type>
void std_node_socket_color_fn(bContext * /*C*/,
                              PointerRNA * /*ptr*/,
                              PointerRNA * /*node_ptr*/,
                              float *r_color)
{
  copy_v4_v4(r_color, std_node_socket_colors[socket_type]);
};
static void std_node_socket_color_simple_fn(const bke::bNodeSocketType *type, float *r_color)
{
  copy_v4_v4(r_color, std_node_socket_colors[type->type]);
};

using SocketColorFn = void (*)(bContext *C, PointerRNA *ptr, PointerRNA *node_ptr, float *r_color);
/* Callbacks for all built-in socket types. */
static const SocketColorFn std_node_socket_color_funcs[] = {
    std_node_socket_color_fn<SOCK_FLOAT>,    std_node_socket_color_fn<SOCK_VECTOR>,
    std_node_socket_color_fn<SOCK_RGBA>,     std_node_socket_color_fn<SOCK_SHADER>,
    std_node_socket_color_fn<SOCK_BOOLEAN>,  nullptr /* UNUSED. */,
    std_node_socket_color_fn<SOCK_INT>,      std_node_socket_color_fn<SOCK_STRING>,
    std_node_socket_color_fn<SOCK_OBJECT>,   std_node_socket_color_fn<SOCK_IMAGE>,
    std_node_socket_color_fn<SOCK_GEOMETRY>, std_node_socket_color_fn<SOCK_COLLECTION>,
    std_node_socket_color_fn<SOCK_TEXTURE>,  std_node_socket_color_fn<SOCK_MATERIAL>,
    std_node_socket_color_fn<SOCK_ROTATION>, std_node_socket_color_fn<SOCK_MENU>,
    std_node_socket_color_fn<SOCK_MATRIX>,   std_node_socket_color_fn<SOCK_BUNDLE>,
    std_node_socket_color_fn<SOCK_CLOSURE>,
};

static bool socket_needs_attribute_search(bNode &node, bNodeSocket &socket)
{
  const nodes::NodeDeclaration *node_decl = node.declaration();
  if (node_decl == nullptr) {
    return false;
  }
  if (node_decl->skip_updating_sockets) {
    return false;
  }
  if (socket.in_out == SOCK_OUT) {
    return false;
  }
  return socket.runtime->declaration->is_attribute_name;
}

static bool socket_needs_layer_search(const bNode &node, const bNodeSocket &socket)
{
  const nodes::NodeDeclaration *node_decl = node.declaration();
  if (node_decl == nullptr) {
    return false;
  }
  if (node_decl->skip_updating_sockets) {
    return false;
  }
  if (socket.in_out == SOCK_OUT) {
    return false;
  }
  return socket.runtime->declaration->is_layer_name;
}

static bool socket_needs_volume_grid_search(const bNode &node, const bNodeSocket &socket)
{
  const nodes::NodeDeclaration *node_decl = node.declaration();
  if (node_decl == nullptr) {
    return false;
  }
  if (node_decl->skip_updating_sockets) {
    return false;
  }
  if (socket.in_out == SOCK_OUT) {
    return false;
  }
  return socket.runtime->declaration->is_volume_grid_name;
}

static void draw_gizmo_pin_icon(uiLayout *layout, PointerRNA *socket_ptr)
{
  layout->prop(socket_ptr, "pin_gizmo", UI_ITEM_NONE, "", ICON_GIZMO);
}

static void draw_node_socket_name_editable(uiLayout *layout,
                                           bNodeSocket *sock,
                                           const StringRef text)
{
  if (sock->runtime->declaration) {
    if (sock->runtime->declaration->socket_name_rna) {
      layout->alignment_set(ui::LayoutAlign::Left);
      layout->emboss_set(ui::EmbossType::None);
      layout->prop((&sock->runtime->declaration->socket_name_rna->owner),
                   sock->runtime->declaration->socket_name_rna->property_name,
                   UI_ITEM_NONE,
                   "",
                   ICON_NONE);
      return;
    }
  }
  layout->label(text, ICON_NONE);
}

static void draw_node_socket_without_value(uiLayout *layout,
                                           bNodeSocket *sock,
                                           const StringRef text)
{
  draw_node_socket_name_editable(layout, sock, text);
}

static void std_node_socket_draw(
    bContext *C, uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, StringRef label)
{
  bNode *node = (bNode *)node_ptr->data;
  bNodeSocket *sock = (bNodeSocket *)ptr->data;
  bNodeTree *tree = reinterpret_cast<bNodeTree *>(ptr->owner_id);
  int type = sock->typeinfo->type;
  // int subtype = sock->typeinfo->subtype;

  const nodes::SocketDeclaration *socket_decl = sock->runtime->declaration;

  if (sock->is_inactive()) {
    layout->active_set(false);
  }

  const bool optional_label = (socket_decl && socket_decl->optional_label) || label.is_empty();
  const StringRef label_or_empty = optional_label ? "" : label;

  const bool has_gizmo = tree->runtime->gizmo_propagation ?
                             tree->runtime->gizmo_propagation->gizmo_endpoint_sockets.contains(
                                 sock) :
                             false;

  if (has_gizmo) {
    if (sock->in_out == SOCK_OUT && node->is_group_input()) {
      uiLayout *row = &layout->row(false);
      row->alignment_set(ui::LayoutAlign::Right);
      node_socket_button_label(C, row, ptr, node_ptr, label);
      row->label("", ICON_GIZMO);
      return;
    }
    if (sock->in_out == SOCK_IN && sock->index() == 0 &&
        nodes::gizmos::is_builtin_gizmo_node(*node))
    {
      uiLayout *row = &layout->row(false);
      node_socket_button_label(C, row, ptr, node_ptr, label);
      draw_gizmo_pin_icon(row, ptr);
      return;
    }
  }

  if ((sock->in_out == SOCK_OUT) || (sock->flag & SOCK_HIDE_VALUE) || sock->is_logically_linked())
  {
    draw_node_socket_without_value(layout, sock, label);
    return;
  }
  if (tree->type == NTREE_GEOMETRY &&
      ELEM(sock->display_shape, SOCK_DISPLAY_SHAPE_LIST, SOCK_DISPLAY_SHAPE_VOLUME_GRID))
  {
    draw_node_socket_without_value(layout, sock, label);
    return;
  }

  /* Some socket types draw the gizmo icon in a special way to look better. All others use a
   * fallback default code path. */
  bool gizmo_handled = false;

  switch (type) {
    case SOCK_FLOAT:
    case SOCK_INT:
    case SOCK_BOOLEAN:
      layout->prop(ptr, "default_value", DEFAULT_FLAGS, label_or_empty, ICON_NONE);
      break;
    case SOCK_VECTOR:
      if (socket_decl && socket_decl->compact) {
        uiTemplateComponentMenu(layout, ptr, "default_value", label_or_empty);
      }
      else {
        if (sock->typeinfo->subtype == PROP_DIRECTION) {
          layout->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
        }
        else {
          uiLayout *column = &layout->column(false);
          {
            uiLayout *row = &column->row(true);
            draw_node_socket_name_editable(row, sock, label_or_empty);
            if (has_gizmo) {
              draw_gizmo_pin_icon(row, ptr);
              gizmo_handled = true;
            }
          }
          column->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
        }
      }
      break;
    case SOCK_ROTATION: {
      uiLayout *column = &layout->column(false);
      {
        uiLayout *row = &column->row(true);
        draw_node_socket_name_editable(row, sock, label_or_empty);
        if (has_gizmo) {
          draw_gizmo_pin_icon(row, ptr);
          gizmo_handled = true;
        }
      }
      column->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
      break;
    }
    case SOCK_MATRIX: {
      draw_node_socket_name_editable(layout, sock, label);
      break;
    }
    case SOCK_RGBA: {
      if (optional_label) {
        layout->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
      }
      else {
        uiLayout *row = &layout->split(0.4f, false);
        uiLayout *label_layout = &row->column(true);
        label_layout->label(label, ICON_NONE);
        uiLayout *color_layout = &row->column(true);
        color_layout->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
        /* Keep color layout active to avoid darkened color appearance when inactive. */
        if (sock->is_inactive()) {
          layout->active_set(true);
          label_layout->active_set(false);
          color_layout->active_set(true);
        }
      }
      break;
    }
    case SOCK_STRING: {
      if (socket_needs_attribute_search(*node, *sock)) {
        if (optional_label) {
          node_geometry_add_attribute_search_button(*C, *node, *ptr, *layout, label);
        }
        else {
          uiLayout *row = &layout->split(0.4f, false);
          row->label(label, ICON_NONE);
          node_geometry_add_attribute_search_button(*C, *node, *ptr, *row);
        }
      }
      else if (socket_needs_layer_search(*node, *sock)) {
        if (optional_label) {
          node_geometry_add_layer_search_button(*C, *node, *ptr, *layout, label);
        }
        else {
          uiLayout *row = &layout->split(0.4f, false);
          row->label(label, ICON_NONE);
          node_geometry_add_layer_search_button(*C, *node, *ptr, *row);
        }
      }
      else if (socket_needs_volume_grid_search(*node, *sock)) {
        if (optional_label) {
          node_geometry_add_volume_grid_search_button(*C, *node, *ptr, *layout, label);
        }
        else {
          uiLayout *row = &layout->split(0.4f, false);
          row->label(label, ICON_NONE);
          node_geometry_add_volume_grid_search_button(*C, *node, *ptr, *row);
        }
      }
      else {
        if (optional_label) {
          layout->prop(ptr,
                       RNA_struct_find_property(ptr, "default_value"),
                       -1,
                       0,
                       UI_ITEM_NONE,
                       "",
                       ICON_NONE,
                       label);
        }
        else {
          uiLayout *row = &layout->split(0.4f, false);
          row->label(label, ICON_NONE);
          row->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
        }
      }
      break;
    }
    case SOCK_MENU: {
      const bNodeSocketValueMenu *default_value =
          sock->default_value_typed<bNodeSocketValueMenu>();
      if (default_value->enum_items) {
        if (default_value->enum_items->items.is_empty()) {
          uiLayout *row = &layout->split(0.4f, false);
          row->label(label, ICON_NONE);
          row->label(IFACE_("No Items"), ICON_NONE);
        }
        else {
          bool expanded = false;
          if (const auto *menu_decl = dynamic_cast<const nodes::decl::Menu *>(socket_decl)) {
            expanded = menu_decl->is_expanded;
          }
          if (optional_label) {
            if (expanded) {
              /* Use a single space for the name to work around a bug. Also see
               * #ui_item_enum_expand_exec. */
              layout->prop(ptr, "default_value", UI_ITEM_R_EXPAND, " ", ICON_NONE);
            }
            else {
              layout->prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
            }
          }
          else {
            uiLayout &row = layout->split(0.4f, false);
            row.label(label, ICON_NONE);
            if (expanded) {
              /* Use a single space for the name to work around a bug. Also see
               * #ui_item_enum_expand_exec. */
              row.row(true).prop(ptr, "default_value", UI_ITEM_R_EXPAND, " ", ICON_NONE);
            }
            else {
              row.prop(ptr, "default_value", DEFAULT_FLAGS, "", ICON_NONE);
            }
          }
        }
      }
      else if (default_value->has_conflict()) {
        layout->label(IFACE_("Menu Error"), ICON_ERROR);
      }
      else {
        layout->label(IFACE_("Menu Undefined"), ICON_QUESTION);
      }
      break;
    }
    case SOCK_COLLECTION:
    case SOCK_OBJECT:
    case SOCK_MATERIAL: {
      if (optional_label) {
        layout->prop(ptr,
                     RNA_struct_find_property(ptr, "default_value"),
                     -1,
                     0,
                     DEFAULT_FLAGS,
                     "",
                     ICON_NONE,
                     std::optional(label));
      }
      else {
        layout->prop(ptr,
                     RNA_struct_find_property(ptr, "default_value"),
                     -1,
                     0,
                     DEFAULT_FLAGS,
                     label,
                     ICON_NONE);
      }

      break;
    }
    case SOCK_IMAGE: {
      const bNodeTree *node_tree = (const bNodeTree *)node_ptr->owner_id;
      if (node_tree->type == NTREE_GEOMETRY) {
        if (optional_label) {
          uiTemplateID(layout, C, ptr, "default_value", "image.new", "image.open", nullptr);
        }
        else {
          /* 0.3 split ratio is inconsistent, but use it here because the "New" button is large. */
          uiLayout *row = &layout->split(0.3f, false);
          row->label(label, ICON_NONE);
          uiTemplateID(row, C, ptr, "default_value", "image.new", "image.open", nullptr);
        }
      }
      else {
        layout->prop(ptr, "default_value", DEFAULT_FLAGS, label_or_empty, ICON_NONE);
      }
      break;
    }
    case SOCK_TEXTURE: {
      if (optional_label) {
        uiTemplateID(layout, C, ptr, "default_value", "texture.new", nullptr, nullptr);
      }
      else {
        /* 0.3 split ratio is inconsistent, but use it here because the "New" button is large. */
        uiLayout *row = &layout->split(0.3f, false);
        row->label(label, ICON_NONE);
        uiTemplateID(row, C, ptr, "default_value", "texture.new", nullptr, nullptr);
      }

      break;
    }
    default:
      draw_node_socket_without_value(layout, sock, label_or_empty);
      break;
  }

  if (has_gizmo && !gizmo_handled) {
    draw_gizmo_pin_icon(layout, ptr);
  }
}

static void std_node_socket_interface_draw(ID *id,
                                           bNodeTreeInterfaceSocket *interface_socket,
                                           bContext * /*C*/,
                                           uiLayout *layout)
{
  PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_NodeTreeInterfaceSocket, interface_socket);

  const bke::bNodeSocketType *typeinfo = interface_socket->socket_typeinfo();
  BLI_assert(typeinfo != nullptr);
  eNodeSocketDatatype type = eNodeSocketDatatype(typeinfo->type);

  uiLayout *col = &layout->column(false);

  switch (type) {
    case SOCK_FLOAT: {
      col->prop(&ptr, "subtype", DEFAULT_FLAGS, IFACE_("Subtype"), ICON_NONE);
      col->prop(&ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      uiLayout *sub = &col->column(true);
      sub->prop(&ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
      sub->prop(&ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);
      break;
    }
    case SOCK_INT: {
      col->prop(&ptr, "subtype", DEFAULT_FLAGS, IFACE_("Subtype"), ICON_NONE);
      col->prop(&ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      uiLayout *sub = &col->column(true);
      sub->prop(&ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
      sub->prop(&ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);
      break;
    }
    case SOCK_VECTOR: {
      col->prop(&ptr, "subtype", DEFAULT_FLAGS, IFACE_("Subtype"), ICON_NONE);
      col->prop(&ptr,
                "dimensions",
                DEFAULT_FLAGS,
                CTX_IFACE_(BLT_I18NCONTEXT_ID_TEXTURE, "Dimensions"),
                ICON_NONE);
      col->prop(&ptr, "default_value", UI_ITEM_R_EXPAND, IFACE_("Default"), ICON_NONE);
      uiLayout *sub = &col->column(true);
      sub->prop(&ptr, "min_value", DEFAULT_FLAGS, IFACE_("Min"), ICON_NONE);
      sub->prop(&ptr, "max_value", DEFAULT_FLAGS, IFACE_("Max"), ICON_NONE);
      break;
    }
    case SOCK_STRING: {
      col->prop(&ptr, "subtype", DEFAULT_FLAGS, IFACE_("Subtype"), ICON_NONE);
      col->prop(&ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      break;
    }
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_RGBA:
    case SOCK_OBJECT:
    case SOCK_COLLECTION:
    case SOCK_IMAGE:
    case SOCK_TEXTURE:
    case SOCK_MATERIAL: {
      col->prop(&ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      break;
    }
    case SOCK_MENU: {
      col->prop(&ptr, "default_value", DEFAULT_FLAGS, IFACE_("Default"), ICON_NONE);
      col->prop(&ptr, "menu_expanded", DEFAULT_FLAGS, IFACE_("Expanded"), ICON_NONE);
      break;
    }
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
    case SOCK_MATRIX:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      break;

    case SOCK_CUSTOM:
      BLI_assert_unreachable();
      break;
  }

  col = &layout->column(false);

  const bNodeTree *node_tree = reinterpret_cast<const bNodeTree *>(id);
  if (interface_socket->flag & NODE_INTERFACE_SOCKET_INPUT && node_tree->type == NTREE_GEOMETRY) {
    if (ELEM(type, SOCK_INT, SOCK_VECTOR, SOCK_MATRIX)) {
      col->prop(&ptr, "default_input", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    }
  }

  if (interface_socket->flag & NODE_INTERFACE_SOCKET_INPUT) {
    col->prop(&ptr, "optional_label", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
  }
  {
    uiLayout *sub = &col->column(false);
    sub->active_set(interface_socket->default_input == NODE_DEFAULT_INPUT_VALUE);
    sub->prop(&ptr, "hide_value", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
  }

  if (interface_socket->flag & NODE_INTERFACE_SOCKET_INPUT && node_tree->type == NTREE_GEOMETRY) {
    if (type == SOCK_BOOLEAN) {
      col->prop(&ptr, "layer_selection_field", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    }
    uiLayout *sub = &col->column(false);
    sub->active_set(!is_layer_selection_field(*interface_socket));
    sub->prop(&ptr, "hide_in_modifier", DEFAULT_FLAGS, std::nullopt, ICON_NONE);
    if (nodes::socket_type_supports_fields(type) || nodes::socket_type_supports_grids(type)) {
      sub->prop(&ptr, "structure_type", DEFAULT_FLAGS, IFACE_("Shape"), ICON_NONE);
    }
  }
}

static void node_socket_virtual_draw_color(bContext * /*C*/,
                                           PointerRNA * /*ptr*/,
                                           PointerRNA * /*node_ptr*/,
                                           float *r_color)
{
  copy_v4_v4(r_color, virtual_node_socket_color);
}

static void node_socket_virtual_draw_color_simple(const bke::bNodeSocketType * /*type*/,
                                                  float *r_color)
{
  copy_v4_v4(r_color, virtual_node_socket_color);
}

}  // namespace blender::ed::space_node

void ED_init_standard_node_socket_type(blender::bke::bNodeSocketType *stype)
{
  using namespace blender::ed::space_node;
  stype->draw = std_node_socket_draw;
  stype->draw_color = std_node_socket_color_funcs[stype->type];
  stype->draw_color_simple = std_node_socket_color_simple_fn;
  stype->interface_draw = std_node_socket_interface_draw;
}

void ED_init_node_socket_type_virtual(blender::bke::bNodeSocketType *stype)
{
  using namespace blender::ed::space_node;
  stype->draw = std_node_socket_draw;
  stype->draw_color = node_socket_virtual_draw_color;
  stype->draw_color_simple = node_socket_virtual_draw_color_simple;
}

void ED_node_type_draw_color(const char *idname, float *r_color)
{
  using namespace blender::ed::space_node;

  const blender::bke::bNodeSocketType *typeinfo = blender::bke::node_socket_type_find(idname);
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
                                                         bke::NODE_INSTANCE_KEY_NONE);
  GPU_matrix_push_projection();
  GPU_matrix_push();
  wmOrtho2_region_pixelspace(&region);
  GPU_matrix_identity_set();
  ED_region_draw_cb_draw(&C, &region, REGION_DRAW_BACKDROP);
  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  if (!(snode.flag & SNODE_BACKDRAW) || !ED_node_is_compositor(&snode) ||
      snode.node_tree_sub_type != SNODE_COMPOSITOR_SCENE)
  {
    return;
  }

  if (parent_key.value != active_viewer_key.value) {
    return;
  }

  GPU_matrix_push_projection();
  GPU_matrix_push();

  /* The draw manager is used to draw the backdrop image. */
  blender::gpu::FrameBuffer *old_fb = GPU_framebuffer_active_get();
  GPU_framebuffer_restore();
  BLI_thread_lock(LOCK_DRAW_IMAGE);
  DRW_draw_view(&C);
  BLI_thread_unlock(LOCK_DRAW_IMAGE);
  GPU_framebuffer_bind_no_srgb(old_fb);
  /* Draw manager changes the depth state. Set it back to NONE. Without this the
   * node preview images aren't drawn correctly. */
  GPU_depth_test(GPU_DEPTH_NONE);

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);
  if (ibuf) {
    /* somehow the offset has to be calculated inverse */
    wmOrtho2_region_pixelspace(&region);
    const float offset_x = snode.xof + ima->runtime->backdrop_offset[0] * snode.zoom;
    const float offset_y = snode.yof + ima->runtime->backdrop_offset[1] * snode.zoom;
    const float x = (region.winx - snode.zoom * ibuf->x) / 2 + offset_x;
    const float y = (region.winy - snode.zoom * ibuf->y) / 2 + offset_y;

    /** \note draw selected info on backdrop
     */
    if (snode.edittree) {
      bNode *node = (bNode *)snode.edittree->nodes.first;
      const rctf *viewer_border = &snode.nodetree->viewer_border;
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
            immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
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

float2 socket_link_connection_location(const bNode &node,
                                       const bNodeSocket &socket,
                                       const bNodeLink &link)
{
  const float2 socket_location = socket.runtime->location;
  if (socket.is_multi_input() && socket.is_input() && !(node.flag & NODE_COLLAPSED)) {
    /* For internal link case, handle number of links as at least 1. */
    const int clamped_total_inputs = math::max<int>(1, socket.runtime->total_inputs);
    return node_link_calculate_multi_input_position(
        socket_location, link.multi_input_sort_id, clamped_total_inputs);
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
    const float slope = math::safe_divide(dist_y, dist_x);
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

/* -------------------------------------------------------------------- */
/** \name Node Socket Drawing
 * \{ */

/* Keep in sync with node socket shader. */
#define MAX_SOCKET_PARAMETERS 4
#define MAX_SOCKET_INSTANCE 32

struct GBatchNodesocket {
  gpu::Batch *batch;
  Vector<NodeSocketShaderParameters, MAX_SOCKET_INSTANCE> params;
  bool enabled;
};

static GBatchNodesocket &g_batch_nodesocket()
{
  static GBatchNodesocket nodesocket_batch;
  return nodesocket_batch;
}

static gpu::Batch *nodesocket_batch_init()
{
  if (g_batch_nodesocket().batch == nullptr) {
    GPUIndexBufBuilder ibuf;
    GPU_indexbuf_init(&ibuf, GPU_PRIM_TRIS, 2, 4);
    /* Quad to draw the node socket in. */
    GPU_indexbuf_add_tri_verts(&ibuf, 0, 1, 2);
    GPU_indexbuf_add_tri_verts(&ibuf, 2, 1, 3);

    g_batch_nodesocket().batch = GPU_batch_create_ex(
        GPU_PRIM_TRIS, nullptr, GPU_indexbuf_build(&ibuf), GPU_BATCH_OWNS_INDEX);
    gpu_batch_presets_register(g_batch_nodesocket().batch);
  }
  return g_batch_nodesocket().batch;
}

static void nodesocket_cache_flush()
{
  if (g_batch_nodesocket().params.is_empty()) {
    return;
  }

  gpu::Batch *batch = nodesocket_batch_init();
  if (g_batch_nodesocket().params.size() == 1) {
    /* draw single */
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_NODE_SOCKET);
    GPU_batch_uniform_4fv_array(
        batch,
        "parameters",
        4,
        reinterpret_cast<const float (*)[4]>(g_batch_nodesocket().params.data()));
    GPU_batch_draw(batch);
  }
  else {
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_NODE_SOCKET_INST);
    GPU_batch_uniform_4fv_array(
        batch,
        "parameters",
        MAX_SOCKET_PARAMETERS * MAX_SOCKET_INSTANCE,
        reinterpret_cast<const float (*)[4]>(g_batch_nodesocket().params.data()));
    GPU_batch_draw_instance_range(batch, 0, g_batch_nodesocket().params.size());
  }
  g_batch_nodesocket().params.clear();
}

void nodesocket_batch_start()
{
  BLI_assert(g_batch_nodesocket().enabled == false);
  g_batch_nodesocket().enabled = true;
}

void nodesocket_batch_end()
{
  BLI_assert(g_batch_nodesocket().enabled == true);
  g_batch_nodesocket().enabled = false;

  GPU_blend(GPU_BLEND_ALPHA);
  nodesocket_cache_flush();
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_node_socket_batch(const NodeSocketShaderParameters &socket_params)
{
  if (g_batch_nodesocket().enabled) {
    g_batch_nodesocket().params.append(socket_params);

    if (g_batch_nodesocket().params.size() >= MAX_SOCKET_INSTANCE) {
      nodesocket_cache_flush();
    }
  }
  else {
    /* Draw single instead of batch. */
    gpu::Batch *batch = nodesocket_batch_init();
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_NODE_SOCKET);
    GPU_batch_uniform_4fv_array(
        batch, "parameters", MAX_SOCKET_PARAMETERS, (const float (*)[4])(&socket_params));
    GPU_batch_draw(batch);
  }
}

void node_draw_nodesocket(const rctf *rect,
                          const float color_inner[4],
                          const float color_outline[4],
                          const float outline_thickness,
                          const int shape,
                          const float aspect)
{
  /* WATCH: This is assuming the ModelViewProjectionMatrix is area pixel space.
   * If it has been scaled, then it's no longer valid. */
  BLI_assert((color_inner != nullptr) && (color_outline != nullptr));

  NodeSocketShaderParameters socket_params = {};
  socket_params.rect[0] = rect->xmin;
  socket_params.rect[1] = rect->xmax;
  socket_params.rect[2] = rect->ymin;
  socket_params.rect[3] = rect->ymax;
  socket_params.color_inner[0] = color_inner[0];
  socket_params.color_inner[1] = color_inner[1];
  socket_params.color_inner[2] = color_inner[2];
  socket_params.color_inner[3] = color_inner[3];
  socket_params.color_outline[0] = color_outline[0];
  socket_params.color_outline[1] = color_outline[1];
  socket_params.color_outline[2] = color_outline[2];
  socket_params.color_outline[3] = color_outline[3];
  socket_params.outline_thickness = outline_thickness;
  socket_params.outline_offset = 0.0;
  socket_params.shape = float(shape) + 0.1f;
  socket_params.aspect = aspect;

  GPU_blend(GPU_BLEND_ALPHA);
  draw_node_socket_batch(socket_params);
  GPU_blend(GPU_BLEND_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Link Drawing
 * \{ */

#define NODELINK_GROUP_SIZE 256
#define LINK_RESOL 24
#define LINK_WIDTH 2.5f
#define ARROW_SIZE (7 * UI_SCALE_FAC)

/* Reroute arrow shape and mute bar. These are expanded here and shrunk in the GLSL code.
 * See: `gpu_shader_2D_nodelink_vert.glsl`. */
static float arrow_verts[3][2] = {{-1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, -1.0f}};
static float arrow_expand_axis[3][2] = {{0.7071f, 0.7071f}, {M_SQRT2, 0.0f}, {0.7071f, -0.7071f}};
static float mute_verts[3][2] = {{0.7071f, 1.0f}, {0.7071f, 0.0f}, {0.7071f, -1.0f}};
static float mute_expand_axis[3][2] = {{1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, -0.0f}};

/* Is zero initialized because it is static data. */
static struct {
  gpu::Batch *batch;
  gpu::StorageBuf *link_buf;
  uint count;
  bool enabled;
  NodeLinkData data[NODELINK_GROUP_SIZE];
} g_batch_link;

static void nodelink_batch_reset()
{
  g_batch_link.count = 0;
}

static void set_nodelink_vertex(gpu::VertBuf *vbo,
                                uint uv_id,
                                uint pos_id,
                                uint exp_id,
                                uint v,
                                const float uv[2],
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
  uint uv_id = GPU_vertformat_attr_add(&format, "uv", gpu::VertAttrType::SFLOAT_32_32);
  uint pos_id = GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  uint expand_id = GPU_vertformat_attr_add(&format, "expand", gpu::VertAttrType::SFLOAT_32_32);
  gpu::VertBuf *vbo = GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC);
  int vcount = LINK_RESOL * 2; /* curve */
  vcount += 2;                 /* restart strip */
  vcount += 3 * 2;             /* arrow */
  vcount += 2;                 /* restart strip */
  vcount += 3 * 2;             /* mute */
  vcount *= 2;                 /* shadow */
  vcount += 2;                 /* restart strip */
  GPU_vertbuf_data_alloc(*vbo, vcount);
  int v = 0;

  for (int k = 0; k < 2; k++) {
    float uv[2] = {0.0f, 0.0f};
    float pos[2] = {0.0f, 0.0f};
    float exp[2] = {0.0f, 1.0f};

    /* restart */
    if (k == 1) {
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }

    /* curve strip */
    for (int i = 0; i < LINK_RESOL; i++) {
      uv[0] = (i / float(LINK_RESOL - 1));
      uv[1] = 0.0f;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
      uv[1] = 1.0f;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }
    /* restart */
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

    uv[0] = 0.5f;
    uv[1] = 0.0f;
    copy_v2_v2(pos, arrow_verts[0]);
    copy_v2_v2(exp, arrow_expand_axis[0]);
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    /* arrow */
    for (int i = 0; i < 3; i++) {
      uv[1] = 0.0f;
      copy_v2_v2(pos, arrow_verts[i]);
      copy_v2_v2(exp, arrow_expand_axis[i]);
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

      uv[1] = 1.0f;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }

    /* restart */
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

    uv[0] = 0.5f;
    uv[1] = 0.0f;
    copy_v2_v2(pos, mute_verts[0]);
    copy_v2_v2(exp, mute_expand_axis[0]);
    set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    /* bar */
    for (int i = 0; i < 3; ++i) {
      uv[1] = 0.0f;
      copy_v2_v2(pos, mute_verts[i]);
      copy_v2_v2(exp, mute_expand_axis[i]);
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);

      uv[1] = 1.0f;
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }

    /* restart */
    if (k == 0) {
      set_nodelink_vertex(vbo, uv_id, pos_id, expand_id, v++, uv, pos, exp);
    }
  }

  g_batch_link.batch = GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  gpu_batch_presets_register(g_batch_link.batch);

  /* Instances data */
  g_batch_link.link_buf = GPU_storagebuf_create(sizeof(NodeLinkData) * NODELINK_GROUP_SIZE);
  gpu_batch_storage_buffer_register(g_batch_link.link_buf);

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
  NodeLinkUniformData node_link_data;

  UI_GetThemeColor4fv(TH_WIRE_INNER, node_link_data.colors[nodelink_get_color_id(TH_WIRE_INNER)]);
  UI_GetThemeColor4fv(TH_WIRE, node_link_data.colors[nodelink_get_color_id(TH_WIRE)]);
  UI_GetThemeColor4fv(TH_ACTIVE, node_link_data.colors[nodelink_get_color_id(TH_ACTIVE)]);
  UI_GetThemeColor4fv(TH_EDGE_SELECT,
                      node_link_data.colors[nodelink_get_color_id(TH_EDGE_SELECT)]);
  UI_GetThemeColor4fv(TH_REDALERT, node_link_data.colors[nodelink_get_color_id(TH_REDALERT)]);
  node_link_data.aspect = snode.runtime->aspect;
  node_link_data.arrow_size = ARROW_SIZE;

  gpu::UniformBuf *ubo = GPU_uniformbuf_create_ex(
      sizeof(node_link_data), &node_link_data, __func__);

  /* TODO(fclem): Update sub. */
  GPU_storagebuf_update(g_batch_link.link_buf, g_batch_link.data);

  GPU_batch_program_set_builtin(g_batch_link.batch, GPU_SHADER_2D_NODELINK);
  GPU_batch_uniformbuf_bind(g_batch_link.batch, "link_uniforms", ubo);
  GPU_storagebuf_bind(g_batch_link.link_buf, 0);
  GPU_batch_draw_instance_range(g_batch_link.batch, 0, g_batch_link.count);

  GPU_uniformbuf_unbind(ubo);
  GPU_uniformbuf_free(ubo);

  nodelink_batch_reset();

  GPU_blend(GPU_BLEND_NONE);
}

void nodelink_batch_start(const SpaceNode & /*snode*/)
{
  g_batch_link.enabled = true;
}

void nodelink_batch_end(const SpaceNode &snode)
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

  bool draw_arrow;
  bool draw_muted;
  bool highlighted;
  bool has_back_link;

  float dim_factor;
  float thickness;
  float dash_length;
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

  NodeLinkData &data = g_batch_link.data[g_batch_link.count++];
  data.bezier_P0 = points[0];
  data.bezier_P1 = points[1];
  data.bezier_P2 = points[2];
  data.bezier_P3 = points[3];

  char *colid = reinterpret_cast<char *>(&data.color_ids);
  colid[0] = nodelink_get_color_id(draw_config.th_col1);
  colid[1] = nodelink_get_color_id(draw_config.th_col2);
  colid[2] = nodelink_get_color_id(draw_config.th_col3);

  data.do_muted = draw_config.draw_muted;
  data.do_arrow = draw_config.draw_arrow;
  data.start_color = float4(draw_config.start_color);
  data.end_color = float4(draw_config.end_color);
  data.dim_factor = draw_config.dim_factor;
  data.thickness = draw_config.thickness;
  data.dash_length = draw_config.dash_length;
  data.dash_factor = draw_config.dash_factor;
  data.dash_alpha = draw_config.dash_alpha;
  data.has_back_link = draw_config.has_back_link;

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
  const bNodeTree &tree = *snode.edittree;
  if (tree.type != NTREE_GEOMETRY) {
    return false;
  }
  if (link.fromsock && link.fromsock->may_be_field()) {
    return true;
  }
  return false;
}

static bool node_link_is_gizmo_link(const SpaceNode &snode, const bNodeLink &link)
{
  if (snode.edittree->type != NTREE_GEOMETRY) {
    return false;
  }
  if (!link.fromsock || !link.tosock) {
    return false;
  }
  const bNodeTree &tree = *snode.edittree;
  return tree.runtime->sockets_on_active_gizmo_paths.contains(link.fromsock) &&
         tree.runtime->sockets_on_active_gizmo_paths.contains(link.tosock);
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

  draw_config.dim_factor = selected ? 1.0f : node_link_dim_factor(v2d, link);

  bTheme *btheme = UI_GetTheme();
  draw_config.dash_alpha = btheme->space_node.dash_alpha;

  const bool field_link = node_link_is_field_link(snode, link);
  const bool gizmo_link = node_link_is_gizmo_link(snode, link);

  draw_config.dash_factor = field_link ? 0.75f : 1.0f;
  draw_config.dash_length = 10.0f * UI_SCALE_FAC;

  const float scale = UI_view2d_scale_get_x(&v2d);
  /* Clamp the thickness to make the links more readable when zooming out. */
  draw_config.thickness = LINK_WIDTH * max_ff(UI_SCALE_FAC * scale, 1.0f) *
                          (field_link ? 0.7f : 1.0f);
  draw_config.has_back_link = gizmo_link;
  draw_config.highlighted = link.flag & NODE_LINK_TEMP_HIGHLIGHT;
  draw_config.draw_arrow = ((link.tonode && link.tonode->is_reroute()) &&
                            (link.fromnode && link.fromnode->is_reroute()));
  draw_config.draw_muted = (link.flag & NODE_LINK_MUTED);

  UI_GetThemeColor4fv(th_col3, draw_config.outline_color);

  if (snode.overlay.flag & SN_OVERLAY_SHOW_OVERLAYS &&
      snode.overlay.flag & SN_OVERLAY_SHOW_WIRE_COLORS)
  {
    const bNodeTree &node_tree = *snode.edittree;
    PointerRNA from_node_ptr = RNA_pointer_create_discrete(
        &const_cast<ID &>(node_tree.id), &RNA_Node, link.fromnode);
    PointerRNA to_node_ptr = RNA_pointer_create_discrete(
        &const_cast<ID &>(node_tree.id), &RNA_Node, link.tonode);

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
    /* Slow path but should eventually not be the majority of them. */
    nodelink_batch_start(snode);
    nodelink_batch_add_link(snode, points, draw_config);
    nodelink_batch_end(snode);
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
    if (link.flag & NODE_LINK_INSERT_TARGET && !(link.flag & NODE_LINK_INSERT_TARGET_INVALID)) {
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
      C, v2d, snode, link, TH_WIRE_INNER, TH_WIRE_INNER, TH_WIRE, true);
  /* End marker outline. */
  node_draw_link_end_markers(link, draw_config, points, true);
  /* Link. */
  node_draw_link_bezier_ex(snode, draw_config, points);
  /* End marker fill. */
  node_draw_link_end_markers(link, draw_config, points, false);
}

}  // namespace blender::ed::space_node
