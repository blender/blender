/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_vector.hh"

struct bAnimContext;
struct ListBase;

namespace blender::ed::graph {

/**
 * Return all bAnimListElem for which the keyframes are visible in the
 * GUI. This excludes FCurves that are drawn as curves but whose keyframes are NOT shown.
 * All entries of the list will be of type ANIMTYPE_FCURVE.
 *
 * The listbase will have to be freed by the caller with ANIM_animdata_freelist;
 */
ListBase get_editable_fcurves(bAnimContext &ac);

}  // namespace blender::ed::graph
