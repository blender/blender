/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <cmath>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_image.h"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"

#include "IMB_imbuf_types.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "WM_types.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void node_gizmo_calc_matrix_space(const SpaceNode *snode,
                                         const ARegion *region,
                                         float matrix_space[4][4])
{
  unit_m4(matrix_space);
  mul_v3_fl(matrix_space[0], snode->zoom);
  mul_v3_fl(matrix_space[1], snode->zoom);
  matrix_space[3][0] = (region->winx / 2) + snode->xof;
  matrix_space[3][1] = (region->winy / 2) + snode->yof;
}

static void node_gizmo_calc_matrix_space_with_image_dims(const SpaceNode *snode,
                                                         const ARegion *region,
                                                         const float2 &image_dims,
                                                         const float2 &image_offset,
                                                         float matrix_space[4][4])
{
  unit_m4(matrix_space);
  mul_v3_fl(matrix_space[0], snode->zoom * image_dims.x);
  mul_v3_fl(matrix_space[1], snode->zoom * image_dims.y);
  matrix_space[3][0] = ((region->winx / 2) + snode->xof) -
                       ((image_dims.x / 2.0f - image_offset.x) * snode->zoom);
  matrix_space[3][1] = ((region->winy / 2) + snode->yof) -
                       ((image_dims.y / 2.0f - image_offset.y) * snode->zoom);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backdrop Gizmo
 * \{ */

static void gizmo_node_backdrop_prop_matrix_get(const wmGizmo * /*gz*/,
                                                wmGizmoProperty *gz_prop,
                                                void *value_p)
{
  float(*matrix)[4] = (float(*)[4])value_p;
  BLI_assert(gz_prop->type->array_length == 16);
  const SpaceNode *snode = (const SpaceNode *)gz_prop->custom_func.user_data;
  matrix[0][0] = snode->zoom;
  matrix[1][1] = snode->zoom;
  matrix[3][0] = snode->xof;
  matrix[3][1] = snode->yof;
}

static void gizmo_node_backdrop_prop_matrix_set(const wmGizmo * /*gz*/,
                                                wmGizmoProperty *gz_prop,
                                                const void *value_p)
{
  const float(*matrix)[4] = (const float(*)[4])value_p;
  BLI_assert(gz_prop->type->array_length == 16);
  SpaceNode *snode = (SpaceNode *)gz_prop->custom_func.user_data;
  snode->zoom = matrix[0][0];
  snode->xof = matrix[3][0];
  snode->yof = matrix[3][1];
}

static bool WIDGETGROUP_node_transform_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if ((snode->flag & SNODE_BACKDRAW) == 0) {
    return false;
  }

  if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
    bNode *node = bke::nodeGetActive(snode->edittree);

    if (node && ELEM(node->type, CMP_NODE_VIEWER)) {
      return true;
    }
  }

  return false;
}

static void WIDGETGROUP_node_transform_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = (wmGizmoWrapper *)MEM_mallocN(sizeof(wmGizmoWrapper), __func__);

  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(wwrapper->gizmo->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM);

  gzgroup->customdata = wwrapper;
}

static void WIDGETGROUP_node_transform_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  wmGizmo *cage = ((wmGizmoWrapper *)gzgroup->customdata)->gizmo;
  const ARegion *region = CTX_wm_region(C);
  /* center is always at the origin */
  const float origin[3] = {float(region->winx / 2), float(region->winy / 2), 0.0f};

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    const float2 dims = {
        (ibuf->x > 0) ? ibuf->x : 64.0f,
        (ibuf->y > 0) ? ibuf->y : 64.0f,
    };

    RNA_float_set_array(cage->ptr, "dimensions", dims);
    WM_gizmo_set_matrix_location(cage, origin);
    WM_gizmo_set_flag(cage, WM_GIZMO_HIDDEN, false);

    /* Need to set property here for undo. TODO: would prefer to do this in _init. */
    SpaceNode *snode = CTX_wm_space_node(C);
#if 0
    PointerRNA nodeptr = RNA_pointer_create(snode->id, &RNA_SpaceNodeEditor, snode);
    WM_gizmo_target_property_def_rna(cage, "offset", &nodeptr, "backdrop_offset", -1);
    WM_gizmo_target_property_def_rna(cage, "scale", &nodeptr, "backdrop_zoom", -1);
#endif

    wmGizmoPropertyFnParams params{};
    params.value_get_fn = gizmo_node_backdrop_prop_matrix_get;
    params.value_set_fn = gizmo_node_backdrop_prop_matrix_set;
    params.range_get_fn = nullptr;
    params.user_data = snode;
    WM_gizmo_target_property_def_func(cage, "matrix", &params);
  }
  else {
    WM_gizmo_set_flag(cage, WM_GIZMO_HIDDEN, true);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_GGT_backdrop_transform(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Backdrop Transform Widget";
  gzgt->idname = "NODE_GGT_backdrop_transform";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = WIDGETGROUP_node_transform_poll;
  gzgt->setup = WIDGETGROUP_node_transform_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_node_transform_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Crop Gizmo
 * \{ */

struct NodeCropWidgetGroup {
  wmGizmo *border;

  struct {
    float2 dims;
  } state;

  struct {
    PointerRNA ptr;
    PropertyRNA *prop;
    bContext *context;
  } update_data;
};

static void gizmo_node_crop_update(NodeCropWidgetGroup *crop_group)
{
  RNA_property_update(
      crop_group->update_data.context, &crop_group->update_data.ptr, crop_group->update_data.prop);
}

static void two_xy_to_rect(const NodeTwoXYs *nxy, rctf *rect, const float2 &dims, bool is_relative)
{
  if (is_relative) {
    rect->xmin = nxy->fac_x1;
    rect->xmax = nxy->fac_x2;
    rect->ymin = nxy->fac_y1;
    rect->ymax = nxy->fac_y2;
  }
  else {
    rect->xmin = nxy->x1 / dims.x;
    rect->xmax = nxy->x2 / dims.x;
    rect->ymin = nxy->y1 / dims.y;
    rect->ymax = nxy->y2 / dims.y;
  }
}

static void two_xy_from_rect(NodeTwoXYs *nxy,
                             const rctf *rect,
                             const float2 &dims,
                             bool is_relative)
{
  if (is_relative) {
    nxy->fac_x1 = rect->xmin;
    nxy->fac_x2 = rect->xmax;
    nxy->fac_y1 = rect->ymin;
    nxy->fac_y2 = rect->ymax;
  }
  else {
    nxy->x1 = rect->xmin * dims.x;
    nxy->x2 = rect->xmax * dims.x;
    nxy->y1 = rect->ymin * dims.y;
    nxy->y2 = rect->ymax * dims.y;
  }
}

/* scale callbacks */
static void gizmo_node_crop_prop_matrix_get(const wmGizmo *gz,
                                            wmGizmoProperty *gz_prop,
                                            void *value_p)
{
  float(*matrix)[4] = (float(*)[4])value_p;
  BLI_assert(gz_prop->type->array_length == 16);
  NodeCropWidgetGroup *crop_group = (NodeCropWidgetGroup *)gz->parent_gzgroup->customdata;
  const float *dims = crop_group->state.dims;
  const bNode *node = (const bNode *)gz_prop->custom_func.user_data;
  const NodeTwoXYs *nxy = (const NodeTwoXYs *)node->storage;
  bool is_relative = bool(node->custom2);
  rctf rct;
  two_xy_to_rect(nxy, &rct, dims, is_relative);
  matrix[0][0] = fabsf(BLI_rctf_size_x(&rct));
  matrix[1][1] = fabsf(BLI_rctf_size_y(&rct));
  matrix[3][0] = (BLI_rctf_cent_x(&rct) - 0.5f) * dims[0];
  matrix[3][1] = (BLI_rctf_cent_y(&rct) - 0.5f) * dims[1];
}

static void gizmo_node_crop_prop_matrix_set(const wmGizmo *gz,
                                            wmGizmoProperty *gz_prop,
                                            const void *value_p)
{
  const float(*matrix)[4] = (const float(*)[4])value_p;
  BLI_assert(gz_prop->type->array_length == 16);
  NodeCropWidgetGroup *crop_group = (NodeCropWidgetGroup *)gz->parent_gzgroup->customdata;
  const float *dims = crop_group->state.dims;
  bNode *node = (bNode *)gz_prop->custom_func.user_data;
  NodeTwoXYs *nxy = (NodeTwoXYs *)node->storage;
  bool is_relative = bool(node->custom2);
  rctf rct;
  two_xy_to_rect(nxy, &rct, dims, is_relative);
  const bool nx = rct.xmin > rct.xmax;
  const bool ny = rct.ymin > rct.ymax;
  BLI_rctf_resize(&rct, fabsf(matrix[0][0]), fabsf(matrix[1][1]));
  BLI_rctf_recenter(&rct, (matrix[3][0] / dims[0]) + 0.5f, (matrix[3][1] / dims[1]) + 0.5f);
  rctf rct_isect{};
  rct_isect.xmin = 0;
  rct_isect.xmax = 1;
  rct_isect.ymin = 0;
  rct_isect.ymax = 1;
  BLI_rctf_isect(&rct_isect, &rct, &rct);
  if (nx) {
    std::swap(rct.xmin, rct.xmax);
  }
  if (ny) {
    std::swap(rct.ymin, rct.ymax);
  }
  two_xy_from_rect(nxy, &rct, dims, is_relative);
  gizmo_node_crop_update(crop_group);
}

static bool WIDGETGROUP_node_crop_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if ((snode->flag & SNODE_BACKDRAW) == 0) {
    return false;
  }

  if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
    bNode *node = bke::nodeGetActive(snode->edittree);

    if (node && ELEM(node->type, CMP_NODE_CROP)) {
      /* ignore 'use_crop_size', we can't usefully edit the crop in this case. */
      if ((node->custom1 & (1 << 0)) == 0) {
        return true;
      }
    }
  }

  return false;
}

static void WIDGETGROUP_node_crop_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeCropWidgetGroup *crop_group = MEM_new<NodeCropWidgetGroup>(__func__);
  crop_group->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(crop_group->border->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE);

  gzgroup->customdata = crop_group;
}

static void WIDGETGROUP_node_crop_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = (wmGizmo *)gzgroup->gizmos.first;

  SpaceNode *snode = CTX_wm_space_node(C);

  node_gizmo_calc_matrix_space(snode, region, gz->matrix_space);
}

static void WIDGETGROUP_node_crop_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeCropWidgetGroup *crop_group = (NodeCropWidgetGroup *)gzgroup->customdata;
  wmGizmo *gz = crop_group->border;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    crop_group->state.dims[0] = (ibuf->x > 0) ? ibuf->x : 64.0f;
    crop_group->state.dims[1] = (ibuf->y > 0) ? ibuf->y : 64.0f;

    RNA_float_set_array(gz->ptr, "dimensions", crop_group->state.dims);
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

    SpaceNode *snode = CTX_wm_space_node(C);
    bNode *node = bke::nodeGetActive(snode->edittree);

    crop_group->update_data.context = (bContext *)C;
    crop_group->update_data.ptr = RNA_pointer_create(
        (ID *)snode->edittree, &RNA_CompositorNodeCrop, node);
    crop_group->update_data.prop = RNA_struct_find_property(&crop_group->update_data.ptr,
                                                            "relative");

    wmGizmoPropertyFnParams params{};
    params.value_get_fn = gizmo_node_crop_prop_matrix_get;
    params.value_set_fn = gizmo_node_crop_prop_matrix_set;
    params.range_get_fn = nullptr;
    params.user_data = node;
    WM_gizmo_target_property_def_func(gz, "matrix", &params);
  }
  else {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_GGT_backdrop_crop(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Backdrop Crop Widget";
  gzgt->idname = "NODE_GGT_backdrop_crop";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = WIDGETGROUP_node_crop_poll;
  gzgt->setup = WIDGETGROUP_node_crop_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = WIDGETGROUP_node_crop_draw_prepare;
  gzgt->refresh = WIDGETGROUP_node_crop_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sun Beams
 * \{ */

struct NodeSunBeamsWidgetGroup {
  wmGizmo *gizmo;

  struct {
    float2 dims;
    float2 offset;
  } state;
};

static bool WIDGETGROUP_node_sbeam_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if ((snode->flag & SNODE_BACKDRAW) == 0) {
    return false;
  }

  if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
    bNode *node = bke::nodeGetActive(snode->edittree);

    if (node && ELEM(node->type, CMP_NODE_SUNBEAMS)) {
      return true;
    }
  }

  return false;
}

static void WIDGETGROUP_node_sbeam_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeSunBeamsWidgetGroup *sbeam_group = (NodeSunBeamsWidgetGroup *)MEM_mallocN(
      sizeof(NodeSunBeamsWidgetGroup), __func__);

  sbeam_group->gizmo = WM_gizmo_new("GIZMO_GT_move_3d", gzgroup, nullptr);
  wmGizmo *gz = sbeam_group->gizmo;

  RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_CROSS_2D);

  gz->scale_basis = 0.05f / 75.0f;

  gzgroup->customdata = sbeam_group;
}

static void WIDGETGROUP_node_sbeam_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  NodeSunBeamsWidgetGroup *sbeam_group = (NodeSunBeamsWidgetGroup *)gzgroup->customdata;
  ARegion *region = CTX_wm_region(C);
  wmGizmo *gz = (wmGizmo *)gzgroup->gizmos.first;

  SpaceNode *snode = CTX_wm_space_node(C);

  node_gizmo_calc_matrix_space_with_image_dims(
      snode, region, sbeam_group->state.dims, sbeam_group->state.offset, gz->matrix_space);
}

static void WIDGETGROUP_node_sbeam_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeSunBeamsWidgetGroup *sbeam_group = (NodeSunBeamsWidgetGroup *)gzgroup->customdata;
  wmGizmo *gz = sbeam_group->gizmo;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    sbeam_group->state.dims[0] = (ibuf->x > 0) ? ibuf->x : 64.0f;
    sbeam_group->state.dims[1] = (ibuf->y > 0) ? ibuf->y : 64.0f;
    copy_v2_v2(sbeam_group->state.offset, ima->runtime.backdrop_offset);

    SpaceNode *snode = CTX_wm_space_node(C);
    bNode *node = bke::nodeGetActive(snode->edittree);

    /* Need to set property here for undo. TODO: would prefer to do this in _init. */
    PointerRNA nodeptr = RNA_pointer_create(
        (ID *)snode->edittree, &RNA_CompositorNodeSunBeams, node);
    WM_gizmo_target_property_def_rna(gz, "offset", &nodeptr, "source", -1);

    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_MODAL, true);
  }
  else {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_GGT_backdrop_sun_beams(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Sun Beams Widget";
  gzgt->idname = "NODE_GGT_sbeam";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = WIDGETGROUP_node_sbeam_poll;
  gzgt->setup = WIDGETGROUP_node_sbeam_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = WIDGETGROUP_node_sbeam_draw_prepare;
  gzgt->refresh = WIDGETGROUP_node_sbeam_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Corner Pin
 * \{ */

struct NodeCornerPinWidgetGroup {
  wmGizmo *gizmos[4];

  struct {
    float2 dims;
    float2 offset;
  } state;
};

static bool WIDGETGROUP_node_corner_pin_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  if ((snode->flag & SNODE_BACKDRAW) == 0) {
    return false;
  }

  if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
    bNode *node = bke::nodeGetActive(snode->edittree);

    if (node && ELEM(node->type, CMP_NODE_CORNERPIN)) {
      return true;
    }
  }

  return false;
}

static void WIDGETGROUP_node_corner_pin_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  NodeCornerPinWidgetGroup *cpin_group = (NodeCornerPinWidgetGroup *)MEM_mallocN(
      sizeof(NodeCornerPinWidgetGroup), __func__);
  const wmGizmoType *gzt_move_3d = WM_gizmotype_find("GIZMO_GT_move_3d", false);

  for (int i = 0; i < 4; i++) {
    cpin_group->gizmos[i] = WM_gizmo_new_ptr(gzt_move_3d, gzgroup, nullptr);
    wmGizmo *gz = cpin_group->gizmos[i];

    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_CROSS_2D);

    gz->scale_basis = 0.05f / 75.0;
  }

  gzgroup->customdata = cpin_group;
}

static void WIDGETGROUP_node_corner_pin_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  NodeCornerPinWidgetGroup *cpin_group = (NodeCornerPinWidgetGroup *)gzgroup->customdata;
  ARegion *region = CTX_wm_region(C);

  SpaceNode *snode = CTX_wm_space_node(C);

  float matrix_space[4][4];
  node_gizmo_calc_matrix_space_with_image_dims(
      snode, region, cpin_group->state.dims, cpin_group->state.offset, matrix_space);

  for (int i = 0; i < 4; i++) {
    wmGizmo *gz = cpin_group->gizmos[i];
    copy_m4_m4(gz->matrix_space, matrix_space);
  }
}

static void WIDGETGROUP_node_corner_pin_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Main *bmain = CTX_data_main(C);
  NodeCornerPinWidgetGroup *cpin_group = (NodeCornerPinWidgetGroup *)gzgroup->customdata;

  void *lock;
  Image *ima = BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);

  if (ibuf) {
    cpin_group->state.dims[0] = (ibuf->x > 0) ? ibuf->x : 64.0f;
    cpin_group->state.dims[1] = (ibuf->y > 0) ? ibuf->y : 64.0f;
    copy_v2_v2(cpin_group->state.offset, ima->runtime.backdrop_offset);

    SpaceNode *snode = CTX_wm_space_node(C);
    bNode *node = bke::nodeGetActive(snode->edittree);

    /* need to set property here for undo. TODO: would prefer to do this in _init. */
    int i = 0;
    for (bNodeSocket *sock = (bNodeSocket *)node->inputs.first; sock && i < 4; sock = sock->next) {
      if (sock->type == SOCK_VECTOR) {
        wmGizmo *gz = cpin_group->gizmos[i++];

        PointerRNA sockptr = RNA_pointer_create((ID *)snode->edittree, &RNA_NodeSocket, sock);
        WM_gizmo_target_property_def_rna(gz, "offset", &sockptr, "default_value", -1);

        WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_MODAL, true);
      }
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      wmGizmo *gz = cpin_group->gizmos[i];
      WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    }
  }

  BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_GGT_backdrop_corner_pin(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Corner Pin Widget";
  gzgt->idname = "NODE_GGT_backdrop_corner_pin";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = WIDGETGROUP_node_corner_pin_poll;
  gzgt->setup = WIDGETGROUP_node_corner_pin_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = WIDGETGROUP_node_corner_pin_draw_prepare;
  gzgt->refresh = WIDGETGROUP_node_corner_pin_refresh;
}

/** \} */

}  // namespace blender::ed::space_node
