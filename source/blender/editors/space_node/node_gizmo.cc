/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <cmath>

#include "BLI_listbase.hh"
#include "BLI_math_base.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_rect.hh"
#include "BLI_string.hh"
#include "BLI_utildefines.hh"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_node_runtime.hh"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"

#include "IMB_imbuf_types.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "NOD_compositor_gizmos.hh"

#include "WM_types.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Backdrop Gizmo
 * \{ */

void NODE_GGT_backdrop_transform(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Backdrop Transform Widget";
  gzgt->idname = "NODE_GGT_backdrop_transform";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::transform_poll;
  gzgt->setup = nodes::gizmos::transform_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = nodes::gizmos::transform_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Crop Gizmo
 * \{ */

void NODE_GGT_backdrop_crop(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Backdrop Crop Widget";
  gzgt->idname = "NODE_GGT_backdrop_crop";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::crop_poll_space_node;
  gzgt->setup = nodes::gizmos::crop_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = nodes::gizmos::crop_draw_prepare_space_node;
  gzgt->refresh = nodes::gizmos::crop_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Mask
 * \{ */

void NODE_GGT_backdrop_box_mask(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Backdrop Box Mask Widget";
  gzgt->idname = "NODE_GGT_backdrop_box_mask";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::box_mask_poll_space_node;
  gzgt->setup = nodes::gizmos::box_mask_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = nodes::gizmos::bbox_draw_prepare_space_node;
  gzgt->refresh = nodes::gizmos::box_mask_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ellipse Mask
 * \{ */

void NODE_GGT_backdrop_ellipse_mask(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Backdrop Ellipse Mask Widget";
  gzgt->idname = "NODE_GGT_backdrop_ellipse_mask";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::ellipse_mask_poll_space_node;
  gzgt->setup = nodes::gizmos::ellipse_mask_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = nodes::gizmos::bbox_draw_prepare_space_node;
  gzgt->refresh = nodes::gizmos::box_mask_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glare
 * \{ */

void NODE_GGT_backdrop_glare(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Glare Widget";
  gzgt->idname = "NODE_GGT_glare";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::glare_poll_space_node;
  gzgt->setup = nodes::gizmos::glare_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = nodes::gizmos::glare_draw_prepare_space_node;
  gzgt->refresh = nodes::gizmos::glare_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Corner Pin
 * \{ */

void NODE_GGT_backdrop_corner_pin(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Corner Pin Widget";
  gzgt->idname = "NODE_GGT_backdrop_corner_pin";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::corner_pin_poll_space_node;
  gzgt->setup = nodes::gizmos::corner_pin_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = nodes::gizmos::corner_pin_draw_prepare_space_node;
  gzgt->refresh = nodes::gizmos::corner_pin_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split
 * \{ */

void NODE_GGT_backdrop_split(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Split Widget";
  gzgt->idname = "NODE_GGT_backdrop_split";

  gzgt->flag |= WM_GIZMOGROUPTYPE_PERSISTENT;

  gzgt->poll = nodes::gizmos::split_poll_space_node;
  gzgt->setup = nodes::gizmos::split_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = nodes::gizmos::bbox_draw_prepare_space_node;
  gzgt->refresh = nodes::gizmos::split_refresh;
}

/** \} */

}  // namespace blender::ed::space_node
