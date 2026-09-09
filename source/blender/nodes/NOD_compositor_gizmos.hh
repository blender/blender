/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

namespace blender {
struct bContext;
struct wmGizmoGroup;
struct ARegion;
struct wmGizmoGroupType;

namespace nodes::gizmos {

/* -------------------------------------------------------------------- */
/** \name Common BBox Gizmos
 * \{ */

void bbox_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup);
void box_mask_refresh(const bContext *C, wmGizmoGroup *gzgroup);
void bbox_draw_prepare_space_image(const bContext *C, wmGizmoGroup *gzgroup);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Mask
 * \{ */

void box_mask_setup(const bContext *C, wmGizmoGroup *gzgroup);
bool box_mask_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
bool box_mask_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Crop
 * \{ */

void crop_refresh(const bContext *C, wmGizmoGroup *gzgroup);
void crop_setup(const bContext *C, wmGizmoGroup *gzgroup);
bool crop_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
void crop_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup);
bool crop_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glare
 * \{ */

void glare_setup(const bContext *C, wmGizmoGroup *gzgroup);
void glare_refresh(const bContext *C, wmGizmoGroup *gzgroup);
bool glare_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);
void glare_draw_prepare_space_image(const bContext *C, wmGizmoGroup *gzgroup);
bool glare_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
void glare_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Corner Pin
 * \{ */

void corner_pin_setup(const bContext *C, wmGizmoGroup *gzgroup);
void corner_pin_refresh(const bContext *C, wmGizmoGroup *gzgroup);
bool corner_pin_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);
bool corner_pin_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
void corner_pin_draw_prepare_space_image(const bContext *C, wmGizmoGroup *gzgroup);
void corner_pin_draw_prepare_space_node(const bContext *C, wmGizmoGroup *gzgroup);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ellipse Mask
 * \{ */

void ellipse_mask_setup(const bContext *C, wmGizmoGroup *gzgroup);
bool ellipse_mask_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
bool ellipse_mask_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split
 * \{ */

void split_refresh(const bContext *C, wmGizmoGroup *gzgroup);
void split_setup(const bContext *C, wmGizmoGroup *gzgroup);
bool split_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
bool split_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backdrop Gizmo
 * \{ */

bool transform_poll(const bContext *C, wmGizmoGroupType *gzgt);
void transform_refresh(const bContext *C, wmGizmoGroup *gzgroup);
void transform_setup(const bContext *C, wmGizmoGroup *gzgroup);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Translate Gizmo
 * \{ */

void translate_refresh(const bContext *C, wmGizmoGroup *gzgroup);
void translate_setup(const bContext *C, wmGizmoGroup *gzgroup);
bool translate_poll_space_node(const bContext *C, wmGizmoGroupType *gzgt);
bool translate_poll_space_image(const bContext *C, wmGizmoGroupType *gzgt);

/** \} */

}  // namespace nodes::gizmos
}  // namespace blender
