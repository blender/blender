/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase_iterator.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "DNA_listBase.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_gizmo_library.hh"
#include "ED_image.hh"

#include "IMB_imbuf_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "NOD_compositor_gizmos.hh" /* Own include. */

namespace blender::nodes::gizmos {

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static float2 GIZMO_NODE_DEFAULT_DIMS{64.0f, 64.0f};

static float2 node_gizmo_safe_calc_dims(const ImBuf *ibuf, const float2 &fallback_dims)
{
  if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
    return float2{float(ibuf->x), float(ibuf->y)};
  }

  /* We typically want to divide by dims, so avoid returning zero here. */
  BLI_assert(!math::is_any_zero(fallback_dims));
  return fallback_dims;
}

static void node_gizmo_calc_matrix_space_with_image_dims(const ARegion *region,
                                                         const float zoom,
                                                         const float2 space_offset,
                                                         const float2 &image_dims,
                                                         const float2 &image_offset,
                                                         float matrix_space[4][4])
{
  unit_m4(matrix_space);
  mul_v3_fl(matrix_space[0], zoom * image_dims.x);
  mul_v3_fl(matrix_space[1], zoom * image_dims.y);
  matrix_space[3][0] = ((region->winx / 2) + space_offset.x) -
                       ((image_dims.x / 2.0f - image_offset.x) * zoom);
  matrix_space[3][1] = ((region->winy / 2) + space_offset.y) -
                       ((image_dims.y / 2.0f - image_offset.y) * zoom);
}

static void node_gizmo_calc_matrix_space(const ARegion *region,
                                         const float zoom,
                                         const float2 offset,
                                         float matrix_space[4][4])
{
  unit_m4(matrix_space);
  mul_v3_fl(matrix_space[0], zoom);
  mul_v3_fl(matrix_space[1], zoom);
  matrix_space[3][0] = (region->winx / 2) - offset.x;
  matrix_space[3][1] = (region->winy / 2) - offset.y;
}

static bool node_gizmo_is_set_visible(const SpaceNode &snode)
{
  if ((snode.flag & SNODE_BACKDRAW) == 0) {
    return false;
  }

  if (!snode.edittree || snode.edittree->type != NTREE_COMPOSIT) {
    return false;
  }

  if (!(snode.gizmo_flag & (SNODE_GIZMO_HIDE | SNODE_GIZMO_HIDE_ACTIVE_NODE))) {
    return true;
  }

  return false;
}

static bool image_gizmo_is_set_visible(const SpaceImage &sima)
{
  if (!ELEM(sima.mode, SI_MODE_VIEW, SI_MODE_MASK)) {
    return false;
  }

  if (sima.gizmo_flag & SI_GIZMO_HIDE_ACTIVE_NODE) {
    return false;
  }

  Image *image = ED_space_image(&sima);
  if (!(image && image->source == IMA_SRC_VIEWER && image->type == IMA_TYPE_COMPOSITE)) {
    return false;
  }

  return true;
}

static SpaceNode *find_active_node_editor(const bContext *C)
{
  wmWindowManager *window_manager = CTX_wm_manager(C);

  for (wmWindow &window : window_manager->windows) {
    bScreen *screen = WM_window_get_active_screen(&window);
    for (ScrArea &area : screen->areabase) {
      SpaceLink *space_link = static_cast<SpaceLink *>(area.spacedata.first);
      if (!space_link || space_link->spacetype != SPACE_NODE) {
        continue;
      }
      SpaceNode *snode = reinterpret_cast<SpaceNode *>(space_link);
      if (snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
        bNodeTreePath *path = static_cast<bNodeTreePath *>(snode->treepath.last);
        if (snode->nodetree->active_viewer_key == path->parent_key) {
          return snode;
        }
      }
    }
  }

  return nullptr;
}

/** \} */

struct NodeBBoxWidgetGroup {
  wmGizmo *border;

  struct {
    float2 dims;
    float2 offset;
  } state;

  struct {
    PointerRNA ptr;
    PropertyRNA *prop;
    bContext *context;
  } update_data;
};

static bool show_box_mask_gizmo(const SpaceNode &snode)
{
  bNodeTree *node_tree = snode.edittree;
  BLI_assert(node_tree);

  bNode *node = bke::node_get_active(*node_tree);
  if (node == nullptr) {
    return false;
  }

  if (node && node->is_type("CompositorNodeBoxMask")) {
    node_tree->ensure_topology_cache();
    for (bNodeSocket &input : node->inputs) {
      if (STR_ELEM(input.name, "Position", "Size", "Rotation") && input.is_directly_linked()) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool box_mask_poll_space_node(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  return show_box_mask_gizmo(*snode);
}

bool box_mask_poll_space_image(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return false;
  }

  if (!image_gizmo_is_set_visible(*sima)) {
    return false;
  }

  const SpaceNode *snode = find_active_node_editor(C);
  if (snode == nullptr || snode->edittree == nullptr) {
    return false;
  }

  return show_box_mask_gizmo(*snode);
}

void box_mask_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeBBoxWidgetGroup *mask_group = MEM_new<NodeBBoxWidgetGroup>(__func__);
  mask_group->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(mask_group->border->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_ROTATE |
                   ED_GIZMO_CAGE_XFORM_FLAG_SCALE);

  RNA_enum_set(mask_group->border->ptr,
               "draw_options",
               ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE |
                   ED_GIZMO_CAGE_DRAW_FLAG_CORNER_HANDLES);

  gzgroup->customdata = mask_group;
  gzgroup->customdata_free = [](void *customdata) {
    MEM_delete(static_cast<NodeBBoxWidgetGroup *>(customdata));
  };
}

void bbox_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first);

  SpaceNode *snode = CTX_wm_space_node(C);

  node_gizmo_calc_matrix_space(
      region, snode->zoom, float2{-snode->xof, -snode->yof}, gz->matrix_space);
}

void bbox_draw_prepare_space_image(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first);

  SpaceImage *sima = CTX_wm_space_image(C);
  const float2 offset = float2{sima->xof, sima->yof} * sima->zoom;

  node_gizmo_calc_matrix_space(region, sima->zoom, offset, gz->matrix_space);
}

static void gizmo_node_box_mask_prop_matrix_get(const wmGizmo *gz,
                                                wmGizmoProperty *gz_prop,
                                                void *value_p)
{
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  NodeBBoxWidgetGroup *mask_group = static_cast<NodeBBoxWidgetGroup *>(
      gz->parent_gzgroup->customdata);
  const float2 dims = mask_group->state.dims;
  const float2 offset = mask_group->state.offset;
  const bNode *node = static_cast<const bNode *>(gz_prop->custom_func.user_data);
  const float aspect = dims.x / dims.y;

  float loc[3], rot[3][3], size[3];
  mat4_to_loc_rot_size(loc, rot, size, matrix);

  const bNodeSocket *rotation_input = bke::node_find_socket(*node, SOCK_IN, "Rotation");
  const float rotation = rotation_input->default_value_typed<bNodeSocketValueFloat>()->value;
  axis_angle_to_mat3_single(rot, 'Z', rotation);

  const bNodeSocket *position_input = bke::node_find_socket(*node, SOCK_IN, "Position");
  const float2 position = position_input->default_value_typed<bNodeSocketValueVector>()->value;
  loc[0] = (position.x - 0.5) * dims.x + offset.x;
  loc[1] = (position.y - 0.5) * dims.y + offset.y;
  loc[2] = 0;

  const bNodeSocket *size_input = bke::node_find_socket(*node, SOCK_IN, "Size");
  const float2 size_value = size_input->default_value_typed<bNodeSocketValueVector>()->value;
  size[0] = size_value.x;
  size[1] = size_value.y * aspect;
  size[2] = 1;

  loc_rot_size_to_mat4(matrix, loc, rot, size);
}

static void gizmo_node_bbox_update(NodeBBoxWidgetGroup *bbox_group)
{
  RNA_property_update(
      bbox_group->update_data.context, &bbox_group->update_data.ptr, bbox_group->update_data.prop);
}

static void gizmo_node_box_mask_prop_matrix_set(const wmGizmo *gz,
                                                wmGizmoProperty *gz_prop,
                                                const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  NodeBBoxWidgetGroup *mask_group = static_cast<NodeBBoxWidgetGroup *>(
      gz->parent_gzgroup->customdata);
  const float2 dims = mask_group->state.dims;
  const float2 offset = mask_group->state.offset;
  bNode *node = static_cast<bNode *>(gz_prop->custom_func.user_data);

  bNodeSocket *position_input = bke::node_find_socket(*node, SOCK_IN, "Position");
  const float2 position = position_input->default_value_typed<bNodeSocketValueVector>()->value;

  bNodeSocket *size_input = bke::node_find_socket(*node, SOCK_IN, "Size");
  const float2 size_value = size_input->default_value_typed<bNodeSocketValueVector>()->value;

  const float aspect = dims.x / dims.y;
  rctf rct;
  rct.xmin = position.x - size_value.x / 2;
  rct.xmax = position.x + size_value.x / 2;
  rct.ymin = position.y - size_value.y / 2;
  rct.ymax = position.y + size_value.y / 2;

  float loc[3];
  float rot[3][3];
  float size[3];
  mat4_to_loc_rot_size(loc, rot, size, matrix);

  float eul[3];

  /* Rotation can't be extracted from matrix when the gizmo width or height is zero. */
  if (size[0] != 0 and size[1] != 0) {
    mat4_to_eul(eul, matrix);
    bNodeSocket *rotation_input = bke::node_find_socket(*node, SOCK_IN, "Rotation");
    rotation_input->default_value_typed<bNodeSocketValueFloat>()->value = eul[2];
  }

  BLI_rctf_resize(&rct, fabsf(size[0]), fabsf(size[1]) / aspect);
  BLI_rctf_recenter(
      &rct, ((loc[0] - offset.x) / dims.x) + 0.5, ((loc[1] - offset.y) / dims.y) + 0.5);

  size_input->default_value_typed<bNodeSocketValueVector>()->value[0] = size[0];
  size_input->default_value_typed<bNodeSocketValueVector>()->value[1] = size[1] / aspect;
  position_input->default_value_typed<bNodeSocketValueVector>()->value[0] = rct.xmin + size[0] / 2;
  position_input->default_value_typed<bNodeSocketValueVector>()->value[1] = rct.ymin +
                                                                            size[1] / aspect / 2;

  gizmo_node_bbox_update(mask_group);
}

static void gizmo_node_box_mask_foreach_rna_prop(
    wmGizmoProperty *gz_prop,
    const FunctionRef<void(PointerRNA &ptr, PropertyRNA *prop, int index)> callback)
{
  bNode *node = static_cast<bNode *>(gz_prop->custom_func.user_data);

  bNodeSocket *position_socket = bke::node_find_socket(*node, SOCK_IN, "Position");
  bNodeTree &node_tree = node->owner_tree();
  PointerRNA position_ptr = RNA_pointer_create_discrete(
      &node_tree.id, RNA_NodeSocket, position_socket);
  PropertyRNA *position_prop = RNA_struct_find_property(&position_ptr, "default_value");

  bNodeSocket *size_socket = bke::node_find_socket(*node, SOCK_IN, "Size");
  PointerRNA size_ptr = RNA_pointer_create_discrete(&node_tree.id, RNA_NodeSocket, size_socket);
  PropertyRNA *size_prop = RNA_struct_find_property(&size_ptr, "default_value");

  bNodeSocket *rotation_socket = bke::node_find_socket(*node, SOCK_IN, "Rotation");
  PointerRNA rotation_ptr = RNA_pointer_create_discrete(
      &node_tree.id, RNA_NodeSocket, rotation_socket);
  PropertyRNA *rotation_prop = RNA_struct_find_property(&rotation_ptr, "default_value");

  callback(position_ptr, position_prop, -1);
  callback(size_ptr, size_prop, -1);
  callback(rotation_ptr, rotation_prop, 0);
}

void box_mask_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeBBoxWidgetGroup *mask_group = static_cast<NodeBBoxWidgetGroup *>(gzgroup->customdata);
  wmGizmo *gz = mask_group->border;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Render Result");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (UNLIKELY(ibuf == nullptr)) {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    BKE_image_release_ibuf(ima, ibuf, lock);
    return;
  }

  mask_group->state.dims = node_gizmo_safe_calc_dims(ibuf, GIZMO_NODE_DEFAULT_DIMS);
  mask_group->state.offset = ibuf->flags & IB_has_display_window ? float2(ibuf->display_offset) :
                                                                   float2(0.0f);

  RNA_float_set_array(gz->ptr, "dimensions", mask_group->state.dims);
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

  const SpaceNode *snode = find_active_node_editor(C);
  BLI_assert(snode != nullptr);

  bNode *node = bke::node_get_active(*snode->edittree);

  mask_group->update_data.context = const_cast<bContext *>(C);
  bNodeSocket *source_input = bke::node_find_socket(*node, SOCK_IN, "Mask");
  mask_group->update_data.ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(snode->edittree), RNA_NodeSocket, source_input);
  mask_group->update_data.prop = RNA_struct_find_property(&mask_group->update_data.ptr, "enabled");
  BLI_assert(mask_group->update_data.prop != nullptr);

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = gizmo_node_box_mask_prop_matrix_get;
  params.value_set_fn = gizmo_node_box_mask_prop_matrix_set;
  params.range_get_fn = nullptr;
  params.user_data = node;
  params.foreach_rna_prop_fn = gizmo_node_box_mask_foreach_rna_prop;
  WM_gizmo_target_property_def_func(gz, "matrix", &params);

  BKE_image_release_ibuf(ima, ibuf, lock);
}

static void node_input_to_rect(const bNode *node,
                               const float2 &dims,
                               const float2 offset,
                               rctf *r_rect)
{

  const bNodeSocket *x_input = bke::node_find_socket(*node, SOCK_IN, "X");
  PointerRNA x_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(x_input));
  const float xmin = float(RNA_int_get(&x_input_rna_pointer, "default_value"));

  const bNodeSocket *y_input = bke::node_find_socket(*node, SOCK_IN, "Y");
  PointerRNA y_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(y_input));
  const float ymin = float(RNA_int_get(&y_input_rna_pointer, "default_value"));

  const bNodeSocket *width_input = bke::node_find_socket(*node, SOCK_IN, "Width");
  PointerRNA width_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(width_input));
  const float width = float(RNA_int_get(&width_input_rna_pointer, "default_value"));

  const bNodeSocket *height_input = bke::node_find_socket(*node, SOCK_IN, "Height");
  PointerRNA height_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(height_input));
  const float height = float(RNA_int_get(&height_input_rna_pointer, "default_value"));

  r_rect->xmin = (xmin + offset.x) / dims.x;
  r_rect->xmax = (xmin + width + offset.x) / dims.x;
  r_rect->ymin = (ymin + offset.y) / dims.y;
  r_rect->ymax = (ymin + height + offset.y) / dims.y;
}

static void node_input_from_rect(bNode *node,
                                 const rctf *rect,
                                 const float2 &dims,
                                 const float2 &offset)
{
  bNodeSocket *x_input = bke::node_find_socket(*node, SOCK_IN, "X");
  PointerRNA x_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(x_input));

  bNodeSocket *y_input = bke::node_find_socket(*node, SOCK_IN, "Y");
  PointerRNA y_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(y_input));

  bNodeSocket *width_input = bke::node_find_socket(*node, SOCK_IN, "Width");
  PointerRNA width_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(width_input));

  bNodeSocket *height_input = bke::node_find_socket(*node, SOCK_IN, "Height");
  PointerRNA height_input_rna_pointer = RNA_pointer_create_discrete(
      nullptr, RNA_NodeSocket, const_cast<bNodeSocket *>(height_input));

  const float xmin = rect->xmin * dims.x - offset.x;
  const float width = rect->xmax * dims.x - offset.x - xmin;
  const float ymin = rect->ymin * dims.y - offset.y;
  const float height = rect->ymax * dims.y - offset.y - ymin;

  RNA_int_set(&x_input_rna_pointer, "default_value", math::round(xmin));
  RNA_int_set(&y_input_rna_pointer, "default_value", math::round(ymin));
  RNA_int_set(&width_input_rna_pointer, "default_value", math::round(width));
  RNA_int_set(&height_input_rna_pointer, "default_value", math::round(height));
}

static void gizmo_node_crop_prop_matrix_get(const wmGizmo *gz,
                                            wmGizmoProperty *gz_prop,
                                            void *value_p)
{
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  NodeBBoxWidgetGroup *crop_group = static_cast<NodeBBoxWidgetGroup *>(
      gz->parent_gzgroup->customdata);
  const float2 dims = crop_group->state.dims;
  const float2 offset = crop_group->state.offset;
  const bNode *node = static_cast<const bNode *>(gz_prop->custom_func.user_data);

  rctf rct;
  node_input_to_rect(node, dims, offset, &rct);

  matrix[0][0] = fabsf(BLI_rctf_size_x(&rct));
  matrix[1][1] = fabsf(BLI_rctf_size_y(&rct));
  matrix[3][0] = (BLI_rctf_cent_x(&rct) - 0.5f) * dims[0];
  matrix[3][1] = (BLI_rctf_cent_y(&rct) - 0.5f) * dims[1];
}

static void gizmo_node_crop_prop_matrix_set(const wmGizmo *gz,
                                            wmGizmoProperty *gz_prop,
                                            const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  NodeBBoxWidgetGroup *crop_group = static_cast<NodeBBoxWidgetGroup *>(
      gz->parent_gzgroup->customdata);
  const float2 dims = crop_group->state.dims;
  const float2 offset = crop_group->state.offset;
  bNode *node = static_cast<bNode *>(gz_prop->custom_func.user_data);

  rctf rct;
  node_input_to_rect(node, dims, offset, &rct);
  BLI_rctf_resize(&rct, fabsf(matrix[0][0]), fabsf(matrix[1][1]));
  BLI_rctf_recenter(&rct, ((matrix[3][0]) / dims[0]) + 0.5f, ((matrix[3][1]) / dims[1]) + 0.5f);
  rctf rct_isect{};
  rct_isect.xmin = offset.x / dims.x;
  rct_isect.xmax = offset.x / dims.x + 1;
  rct_isect.ymin = offset.y;
  rct_isect.ymax = offset.y / dims.y + 1;
  BLI_rctf_isect(&rct_isect, &rct, &rct);
  node_input_from_rect(node, &rct, dims, offset);
  gizmo_node_bbox_update(crop_group);
}

static bool show_crop_gizmo(const SpaceNode &snode)
{
  bNodeTree *node_tree = snode.edittree;
  BLI_assert(node_tree);

  bNode *node = bke::node_get_active(*node_tree);

  if (!node || !node->is_type("CompositorNodeCrop")) {
    return false;
  }

  node_tree->ensure_topology_cache();
  for (bNodeSocket &input : node->inputs) {
    if (!STREQ(input.name, "Image") && input.is_directly_linked()) {
      /* Note: the Image input could be connected to a single value input, in which case the
       * gizmo has no effect. */
      return false;
    }
    else if (STREQ(input.name, "Alpha Crop") && !input.is_directly_linked()) {
      PointerRNA input_rna_pointer = RNA_pointer_create_discrete(nullptr, RNA_NodeSocket, &input);
      if (RNA_boolean_get(&input_rna_pointer, "default_value")) {
        /* If Alpha Crop is not set, the image size changes depending on the input parameters,
         * so we can't usefully edit the crop in this case. */
        return true;
      }
    }
  }

  return false;
}

bool crop_poll_space_node(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  return show_crop_gizmo(*snode);
}

bool crop_poll_space_image(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return false;
  }

  if (!image_gizmo_is_set_visible(*sima)) {
    return false;
  }

  const SpaceNode *snode = find_active_node_editor(C);
  if (snode == nullptr || snode->edittree == nullptr) {
    return false;
  }

  return show_crop_gizmo(*snode);
}

void crop_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first);

  SpaceNode *snode = CTX_wm_space_node(C);

  node_gizmo_calc_matrix_space(region, snode->zoom, {-snode->xof, -snode->yof}, gz->matrix_space);
}

static void gizmo_node_crop_foreach_rna_prop(
    wmGizmoProperty *gz_prop,
    const FunctionRef<void(PointerRNA &ptr, PropertyRNA *prop, int index)> callback)
{
  bNode *node = static_cast<bNode *>(gz_prop->custom_func.user_data);
  bNodeTree &node_tree = node->owner_tree();

  bNodeSocket *x_socket = bke::node_find_socket(*node, SOCK_IN, "X");
  PointerRNA x_ptr = RNA_pointer_create_discrete(&node_tree.id, RNA_NodeSocket, x_socket);
  PropertyRNA *x_prop = RNA_struct_find_property(&x_ptr, "default_value");

  bNodeSocket *y_socket = bke::node_find_socket(*node, SOCK_IN, "Y");
  PointerRNA y_ptr = RNA_pointer_create_discrete(&node_tree.id, RNA_NodeSocket, y_socket);
  PropertyRNA *y_prop = RNA_struct_find_property(&y_ptr, "default_value");

  bNodeSocket *width_socket = bke::node_find_socket(*node, SOCK_IN, "Width");
  PointerRNA width_ptr = RNA_pointer_create_discrete(&node_tree.id, RNA_NodeSocket, width_socket);
  PropertyRNA *width_prop = RNA_struct_find_property(&width_ptr, "default_value");

  bNodeSocket *height_socket = bke::node_find_socket(*node, SOCK_IN, "Height");
  PointerRNA height_ptr = RNA_pointer_create_discrete(
      &node_tree.id, RNA_NodeSocket, height_socket);
  PropertyRNA *height_prop = RNA_struct_find_property(&height_ptr, "default_value");

  callback(x_ptr, x_prop, 0);
  callback(y_ptr, y_prop, 0);
  callback(width_ptr, width_prop, 0);
  callback(height_ptr, height_prop, 0);
}

void crop_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  const SpaceNode *snode = find_active_node_editor(C);
  BLI_assert(snode != nullptr);

  NodeBBoxWidgetGroup *crop_group = static_cast<NodeBBoxWidgetGroup *>(gzgroup->customdata);
  wmGizmo *gz = crop_group->border;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (UNLIKELY(ibuf == nullptr)) {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    BKE_image_release_ibuf(ima, ibuf, lock);
    return;
  }

  crop_group->state.dims = node_gizmo_safe_calc_dims(ibuf, GIZMO_NODE_DEFAULT_DIMS);
  crop_group->state.offset = ibuf->flags & IB_has_display_window ? float2(ibuf->display_offset) :
                                                                   float2(0.0f);

  RNA_float_set_array(gz->ptr, "dimensions", crop_group->state.dims);
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

  bNode *node = bke::node_get_active(*snode->edittree);

  crop_group->update_data.context = const_cast<bContext *>(C);
  bNodeSocket *source_input = bke::node_find_socket(*node, SOCK_IN, "Alpha Crop");
  crop_group->update_data.ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(snode->edittree), RNA_NodeSocket, source_input);
  crop_group->update_data.prop = RNA_struct_find_property(&crop_group->update_data.ptr, "enabled");
  BLI_assert(crop_group->update_data.prop != nullptr);

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = gizmo_node_crop_prop_matrix_get;
  params.value_set_fn = gizmo_node_crop_prop_matrix_set;
  params.range_get_fn = nullptr;
  params.user_data = node;
  params.foreach_rna_prop_fn = gizmo_node_crop_foreach_rna_prop;
  WM_gizmo_target_property_def_func(gz, "matrix", &params);

  BKE_image_release_ibuf(ima, ibuf, lock);
}

void crop_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeBBoxWidgetGroup *crop_group = MEM_new<NodeBBoxWidgetGroup>(__func__);
  crop_group->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(crop_group->border->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE);
  RNA_enum_set(crop_group->border->ptr,
               "draw_options",
               ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE |
                   ED_GIZMO_CAGE_DRAW_FLAG_CORNER_HANDLES);

  gzgroup->customdata = crop_group;
  gzgroup->customdata_free = [](void *customdata) {
    MEM_delete(static_cast<NodeBBoxWidgetGroup *>(customdata));
  };
}

struct NodeGlareWidgetGroup {
  wmGizmo *gizmo;

  struct {
    float2 dims;
    float2 offset;
  } state;
};

static bool show_glare_gizmo(const SpaceNode &snode)
{
  bNodeTree *node_tree = snode.edittree;
  BLI_assert(node_tree != nullptr);

  bNode *node = bke::node_get_active(*node_tree);

  if (!node || !node->is_type("CompositorNodeGlare")) {
    return false;
  }

  bNodeSocket &type_socket = *bke::node_find_socket(*node, SOCK_IN, "Type");
  node_tree->ensure_topology_cache();
  if (type_socket.is_directly_linked()) {
    return false;
  }

  if (type_socket.default_value_typed<bNodeSocketValueMenu>()->value != CMP_NODE_GLARE_SUN_BEAMS) {
    return false;
  }

  for (bNodeSocket &input : node->inputs) {
    if (STR_ELEM(input.name, "Sun Position") && input.is_directly_linked()) {
      return false;
    }
  }
  return true;
}

bool glare_poll_space_node(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  return show_glare_gizmo(*snode);
}

void glare_draw_prepare_space_image(const bContext *C, wmGizmoGroup *gzgroup)
{

  NodeGlareWidgetGroup *glare_group = static_cast<NodeGlareWidgetGroup *>(gzgroup->customdata);
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first);

  SpaceImage *sima = CTX_wm_space_image(C);
  const float2 offset = float2{-sima->xof, -sima->yof} * sima->zoom;

  node_gizmo_calc_matrix_space_with_image_dims(region,
                                               sima->zoom,
                                               offset,
                                               glare_group->state.dims,
                                               glare_group->state.offset,
                                               gz->matrix_space);
}

bool glare_poll_space_image(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return false;
  }

  if (!image_gizmo_is_set_visible(*sima)) {
    return false;
  }

  const SpaceNode *snode = find_active_node_editor(C);
  if (snode == nullptr || snode->edittree == nullptr) {
    return false;
  }

  return show_glare_gizmo(*snode);
}

void glare_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup)
{

  NodeGlareWidgetGroup *glare_group = static_cast<NodeGlareWidgetGroup *>(gzgroup->customdata);
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first);

  SpaceNode *snode = CTX_wm_space_node(C);

  node_gizmo_calc_matrix_space_with_image_dims(region,
                                               snode->zoom,
                                               {snode->xof, snode->yof},
                                               glare_group->state.dims,
                                               glare_group->state.offset,
                                               gz->matrix_space);
}

void glare_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeGlareWidgetGroup *glare_group = MEM_new_uninitialized<NodeGlareWidgetGroup>(__func__);

  glare_group->gizmo = WM_gizmo_new("GIZMO_GT_move_3d", gzgroup, nullptr);
  wmGizmo *gz = glare_group->gizmo;

  RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_CROSS_2D);

  gz->scale_basis = 0.05f / 75.0f;

  gzgroup->customdata = glare_group;
}

void glare_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeGlareWidgetGroup *glare_group = static_cast<NodeGlareWidgetGroup *>(gzgroup->customdata);
  wmGizmo *gz = glare_group->gizmo;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (UNLIKELY(ibuf == nullptr)) {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    BKE_image_release_ibuf(ima, ibuf, lock);
    return;
  }

  glare_group->state.dims = node_gizmo_safe_calc_dims(ibuf, GIZMO_NODE_DEFAULT_DIMS);
  glare_group->state.offset = ibuf->flags & IB_has_display_window ? float2(ibuf->display_offset) :
                                                                    float2(0.0f);

  SpaceNode *snode = find_active_node_editor(C);
  BLI_assert(snode != nullptr);

  bNode *node = bke::node_get_active(*snode->edittree);

  /* Need to set property here for undo. TODO: would prefer to do this in _init. */
  bNodeSocket *source_input = bke::node_find_socket(*node, SOCK_IN, "Sun Position");
  PointerRNA socket_pointer = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(snode->edittree), RNA_NodeSocket, source_input);
  WM_gizmo_target_property_def_rna(gz, "offset", &socket_pointer, "default_value", -1);

  WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_MODAL, true);

  BKE_image_release_ibuf(ima, ibuf, lock);
}

struct NodeCornerPinWidgetGroup {
  wmGizmo *gizmos[4];

  struct {
    float2 dims;
    float2 offset;
  } state;
};

static bool show_corner_pin(const SpaceNode &snode)
{
  bNode *node = bke::node_get_active(*snode.edittree);

  if (node && node->is_type("CompositorNodeCornerPin")) {
    return true;
  }

  return false;
}

bool corner_pin_poll_space_node(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  return show_corner_pin(*snode);
}

bool corner_pin_poll_space_image(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return false;
  }

  if (!image_gizmo_is_set_visible(*sima)) {
    return false;
  }

  const SpaceNode *snode = find_active_node_editor(C);
  if (snode == nullptr || snode->edittree == nullptr) {
    return false;
  }

  return show_corner_pin(*snode);
}

void corner_pin_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup)
{
  NodeCornerPinWidgetGroup *cpin_group = static_cast<NodeCornerPinWidgetGroup *>(
      gzgroup->customdata);
  ARegion *region = CTX_wm_region(C);

  SpaceNode *snode = CTX_wm_space_node(C);

  float matrix_space[4][4];
  node_gizmo_calc_matrix_space_with_image_dims(region,
                                               snode->zoom,
                                               {snode->xof, snode->yof},
                                               cpin_group->state.dims,
                                               cpin_group->state.offset,
                                               matrix_space);

  for (int i = 0; i < 4; i++) {
    wmGizmo *gz = cpin_group->gizmos[i];
    copy_m4_m4(gz->matrix_space, matrix_space);
  }
}

void corner_pin_draw_prepare_space_image(const bContext *C, wmGizmoGroup *gzgroup)
{

  ARegion *region = CTX_wm_region(C);
  SpaceImage *sima = CTX_wm_space_image(C);

  NodeCornerPinWidgetGroup *cpin_group = static_cast<NodeCornerPinWidgetGroup *>(
      gzgroup->customdata);

  const float2 offset = float2{-sima->xof, -sima->yof} * sima->zoom;

  for (wmGizmo &gz : gzgroup->gizmos) {
    node_gizmo_calc_matrix_space_with_image_dims(region,
                                                 sima->zoom,
                                                 offset,
                                                 cpin_group->state.dims,
                                                 cpin_group->state.offset,
                                                 gz.matrix_space);
  }
}

void corner_pin_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeCornerPinWidgetGroup *cpin_group = MEM_new_uninitialized<NodeCornerPinWidgetGroup>(__func__);
  const wmGizmoType *gzt_move_3d = WM_gizmotype_find("GIZMO_GT_move_3d", false);

  for (int i = 0; i < 4; i++) {
    cpin_group->gizmos[i] = WM_gizmo_new_ptr(gzt_move_3d, gzgroup, nullptr);
    wmGizmo *gz = cpin_group->gizmos[i];

    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_CROSS_2D);

    gz->scale_basis = 0.05f / 75.0;
  }

  gzgroup->customdata = cpin_group;
}

void corner_pin_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeCornerPinWidgetGroup *cpin_group = static_cast<NodeCornerPinWidgetGroup *>(
      gzgroup->customdata);

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (UNLIKELY(ibuf == nullptr)) {
    for (int i = 0; i < 4; i++) {
      wmGizmo *gz = cpin_group->gizmos[i];
      WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    }
    BKE_image_release_ibuf(ima, ibuf, lock);
    return;
  }

  cpin_group->state.dims = node_gizmo_safe_calc_dims(ibuf, GIZMO_NODE_DEFAULT_DIMS);
  cpin_group->state.offset = ibuf->flags & IB_has_display_window ? float2(ibuf->display_offset) :
                                                                   float2(0.0f);

  SpaceNode *snode = find_active_node_editor(C);
  BLI_assert(snode != nullptr);

  bNode *node = bke::node_get_active(*snode->edittree);

  /* need to set property here for undo. TODO: would prefer to do this in _init. */
  int i = 0;
  for (bNodeSocket *sock = static_cast<bNodeSocket *>(node->inputs.first); sock && i < 4;
       sock = sock->next)
  {
    if (sock->type == SOCK_VECTOR) {
      wmGizmo *gz = cpin_group->gizmos[i++];

      PointerRNA sockptr = RNA_pointer_create_discrete(
          id_cast<ID *>(snode->edittree), RNA_NodeSocket, sock);
      WM_gizmo_target_property_def_rna(gz, "offset", &sockptr, "default_value", -1);

      WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_MODAL, true);
    }
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

static bool show_ellipse_mask_gizmo(const SpaceNode &snode)
{
  bNode *node = bke::node_get_active(*snode.edittree);

  if (node && node->is_type("CompositorNodeEllipseMask")) {
    snode.edittree->ensure_topology_cache();
    for (bNodeSocket &input : node->inputs) {
      if (STR_ELEM(input.name, "Position", "Size", "Rotation") && input.is_directly_linked()) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool ellipse_mask_poll_space_node(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  return show_ellipse_mask_gizmo(*snode);
}

bool ellipse_mask_poll_space_image(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return false;
  }

  if (!image_gizmo_is_set_visible(*sima)) {
    return false;
  }

  const SpaceNode *snode = find_active_node_editor(C);
  if (snode == nullptr || snode->edittree == nullptr) {
    return false;
  }

  return show_ellipse_mask_gizmo(*snode);
}

void ellipse_mask_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeBBoxWidgetGroup *mask_group = MEM_new<NodeBBoxWidgetGroup>(__func__);
  mask_group->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(mask_group->border->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_ROTATE |
                   ED_GIZMO_CAGE_XFORM_FLAG_SCALE);
  RNA_enum_set(mask_group->border->ptr, "draw_style", ED_GIZMO_CAGE2D_STYLE_CIRCLE);
  RNA_enum_set(mask_group->border->ptr,
               "draw_options",
               ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE |
                   ED_GIZMO_CAGE_DRAW_FLAG_CORNER_HANDLES);

  gzgroup->customdata = mask_group;
  gzgroup->customdata_free = [](void *customdata) {
    MEM_delete(static_cast<NodeBBoxWidgetGroup *>(customdata));
  };
}

static void gizmo_node_split_foreach_rna_prop(
    wmGizmoProperty *gz_prop,
    const FunctionRef<void(PointerRNA &ptr, PropertyRNA *prop, int index)> callback)
{
  bNode *node = static_cast<bNode *>(gz_prop->custom_func.user_data);

  bNodeSocket *position_socket = bke::node_find_socket(*node, SOCK_IN, "Position");
  bNodeTree &node_tree = node->owner_tree();
  PointerRNA position_ptr = RNA_pointer_create_discrete(
      &node_tree.id, RNA_NodeSocket, position_socket);
  PropertyRNA *position_prop = RNA_struct_find_property(&position_ptr, "default_value");

  bNodeSocket *rotation_socket = bke::node_find_socket(*node, SOCK_IN, "Rotation");
  PointerRNA rotation_ptr = RNA_pointer_create_discrete(
      &node_tree.id, RNA_NodeSocket, rotation_socket);
  PropertyRNA *rotation_prop = RNA_struct_find_property(&rotation_ptr, "default_value");

  callback(position_ptr, position_prop, -1);
  callback(rotation_ptr, rotation_prop, 0);
}

static void gizmo_node_split_prop_matrix_get(const wmGizmo *gz,
                                             wmGizmoProperty *gz_prop,
                                             void *value_p)
{
  float (*matrix)[4] = reinterpret_cast<float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  NodeBBoxWidgetGroup *split_group = static_cast<NodeBBoxWidgetGroup *>(
      gz->parent_gzgroup->customdata);
  const float2 dims = split_group->state.dims;
  const float2 offset = split_group->state.offset;
  const bNode *node = static_cast<const bNode *>(gz_prop->custom_func.user_data);

  float loc[3], rot[3][3], size[3];
  mat4_to_loc_rot_size(loc, rot, size, matrix);

  const bNodeSocket *pos_input = bke::node_find_socket(*node, SOCK_IN, "Position");
  const float2 pos = pos_input->default_value_typed<bNodeSocketValueVector>()->value;

  const bNodeSocket *rotation_input = bke::node_find_socket(*node, SOCK_IN, "Rotation");
  const float rotation = rotation_input->default_value_typed<bNodeSocketValueFloat>()->value;

  const float gizmo_width = 0.1f;
  axis_angle_to_mat3_single(rot, 'Z', rotation);
  loc_rot_size_to_mat4(
      matrix,
      float3{(pos.x - 0.5f) * dims.x + offset.x, (pos.y - 0.5f) * dims.y + offset.y, 0.0f},
      rot,
      float3{gizmo_width, std::numeric_limits<float>::epsilon(), 1.0f});
}

static void gizmo_node_split_prop_matrix_set(const wmGizmo *gz,
                                             wmGizmoProperty *gz_prop,
                                             const void *value_p)
{
  const float (*matrix)[4] = reinterpret_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  NodeBBoxWidgetGroup *split_group = reinterpret_cast<NodeBBoxWidgetGroup *>(
      gz->parent_gzgroup->customdata);
  const float2 dims = split_group->state.dims;
  const float2 offset = split_group->state.offset;
  bNode *node = reinterpret_cast<bNode *>(gz_prop->custom_func.user_data);

  bNodeSocket *position_input = bke::node_find_socket(*node, SOCK_IN, "Position");
  bNodeSocket *rotation_input = bke::node_find_socket(*node, SOCK_IN, "Rotation");

  float pos_x = (matrix[3][0] - offset.x) + dims.x * 0.5;
  float pos_y = (matrix[3][1] - offset.y) + dims.y * 0.5;

  /* Prevent dragging the gizmo outside the image. */
  pos_x = math::clamp(pos_x, 0.0f, dims.x);
  pos_y = math::clamp(pos_y, 0.0f, dims.y);

  position_input->default_value_typed<bNodeSocketValueVector>()->value[0] = pos_x / dims.x;
  position_input->default_value_typed<bNodeSocketValueVector>()->value[1] = pos_y / dims.y;

  float3 eul;
  mat4_to_eul(eul, matrix);

  rotation_input->default_value_typed<bNodeSocketValueFloat>()->value = eul[2];

  gizmo_node_bbox_update(split_group);
}

static bool show_split(const SpaceNode &snode)
{
  bNode *node = bke::node_get_active(*snode.edittree);

  if (node && node->is_type("CompositorNodeSplit")) {
    snode.edittree->ensure_topology_cache();
    for (bNodeSocket &input : node->inputs) {
      if (STR_ELEM(input.name, "Position", "Rotation") && input.is_directly_linked()) {
        return false;
      }
    }
    return true;
  }

  return false;
}

void split_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeBBoxWidgetGroup *split_group = reinterpret_cast<NodeBBoxWidgetGroup *>(gzgroup->customdata);
  wmGizmo *gz = split_group->border;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Render Result");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (UNLIKELY(ibuf == nullptr)) {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    BKE_image_release_ibuf(ima, ibuf, lock);
    return;
  }

  /* Larger fallback size otherwise the gizmo would be partially hidden. */
  split_group->state.dims = node_gizmo_safe_calc_dims(ibuf, float2{1000.0f, 1000.0f});
  split_group->state.offset = ibuf->flags & IB_has_display_window ? float2(ibuf->display_offset) :
                                                                    float2(0.0f);

  RNA_float_set_array(gz->ptr, "dimensions", split_group->state.dims);
  WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

  SpaceNode *snode = find_active_node_editor(C);
  BLI_assert(snode != nullptr);

  bNode *node = bke::node_get_active(*snode->edittree);

  split_group->update_data.context = const_cast<bContext *>(C);
  bNodeSocket *source_input = bke::node_find_socket(*node, SOCK_IN, "Position");
  split_group->update_data.ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(snode->edittree), RNA_NodeSocket, source_input);
  split_group->update_data.prop = RNA_struct_find_property(&split_group->update_data.ptr,
                                                           "enabled");

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = gizmo_node_split_prop_matrix_get;
  params.value_set_fn = gizmo_node_split_prop_matrix_set;
  params.range_get_fn = nullptr;
  params.user_data = node;
  params.foreach_rna_prop_fn = gizmo_node_split_foreach_rna_prop;
  WM_gizmo_target_property_def_func(gz, "matrix", &params);

  BKE_image_release_ibuf(ima, ibuf, lock);
}

bool split_poll_space_node(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  return show_split(*snode);
}

bool split_poll_space_image(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return false;
  }

  if (!image_gizmo_is_set_visible(*sima)) {
    return false;
  }

  const SpaceNode *snode = find_active_node_editor(C);
  if (snode == nullptr || snode->edittree == nullptr) {
    return false;
  }

  return show_split(*snode);
}

void split_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeBBoxWidgetGroup *split_group = MEM_new<NodeBBoxWidgetGroup>(__func__);
  split_group->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(split_group->border->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_ROTATE);
  RNA_enum_set(split_group->border->ptr, "draw_options", ED_GIZMO_CAGE_DRAW_FLAG_NOP);

  gzgroup->customdata = split_group;
  gzgroup->customdata_free = [](void *customdata) {
    MEM_delete(static_cast<NodeBBoxWidgetGroup *>(customdata));
  };
}

static void gizmo_node_backdrop_prop_matrix_get(const wmGizmo * /*gz*/,
                                                wmGizmoProperty *gz_prop,
                                                void *value_p)
{
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  const SpaceNode *snode = static_cast<const SpaceNode *>(gz_prop->custom_func.user_data);
  matrix[0][0] = snode->zoom;
  matrix[1][1] = snode->zoom;
  matrix[3][0] = snode->xof;
  matrix[3][1] = snode->yof;
}

static void gizmo_node_backdrop_prop_matrix_set(const wmGizmo * /*gz*/,
                                                wmGizmoProperty *gz_prop,
                                                const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  SpaceNode *snode = static_cast<SpaceNode *>(gz_prop->custom_func.user_data);
  snode->zoom = matrix[0][0];
  snode->xof = matrix[3][0];
  snode->yof = matrix[3][1];
}

bool transform_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  if (!node_gizmo_is_set_visible(*snode)) {
    return false;
  }

  bNode *node = bke::node_get_active(*snode->edittree);

  if (node && node->is_type("CompositorNodeViewer")) {
    return true;
  }

  return false;
}

void transform_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_new_uninitialized<wmGizmoWrapper>(__func__);

  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(wwrapper->gizmo->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM);
  RNA_enum_set(wwrapper->gizmo->ptr,
               "draw_options",
               ED_GIZMO_CAGE_DRAW_FLAG_XFORM_CENTER_HANDLE |
                   ED_GIZMO_CAGE_DRAW_FLAG_CORNER_HANDLES);

  gzgroup->customdata = wwrapper;
}

void transform_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  wmGizmo *cage = (static_cast<wmGizmoWrapper *>(gzgroup->customdata))->gizmo;
  const ARegion *region = CTX_wm_region(C);
  /* center is always at the origin */
  const float origin[3] = {float(region->winx / 2), float(region->winy / 2), 0.0f};

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (UNLIKELY(ibuf == nullptr)) {
    WM_gizmo_set_flag(cage, WM_GIZMO_HIDDEN, true);
    BKE_image_release_ibuf(ima, ibuf, lock);
    return;
  }

  const float2 dims = node_gizmo_safe_calc_dims(ibuf, GIZMO_NODE_DEFAULT_DIMS);

  RNA_float_set_array(cage->ptr, "dimensions", dims);
  WM_gizmo_set_matrix_location(cage, origin);
  WM_gizmo_set_flag(cage, WM_GIZMO_HIDDEN, false);

  /* Need to set property here for undo. TODO: would prefer to do this in _init. */
  SpaceNode *snode = CTX_wm_space_node(C);
#if 0
  PointerRNA nodeptr = RNA_pointer_create_discrete(snode->id, RNA_SpaceNodeEditor, snode);
  WM_gizmo_target_property_def_rna(cage, "offset", &nodeptr, "backdrop_offset", -1);
  WM_gizmo_target_property_def_rna(cage, "scale", &nodeptr, "backdrop_zoom", -1);
#endif

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = gizmo_node_backdrop_prop_matrix_get;
  params.value_set_fn = gizmo_node_backdrop_prop_matrix_set;
  params.range_get_fn = nullptr;
  params.user_data = snode;
  WM_gizmo_target_property_def_func(cage, "matrix", &params);

  BKE_image_release_ibuf(ima, ibuf, lock);
}

}  // namespace blender::nodes::gizmos
