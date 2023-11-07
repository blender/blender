/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to modify FCurves.
 */

struct AnimData;
struct FCurve;

namespace blender::animrig {
/**
 * \note The caller needs to run #BKE_nla_tweakedit_remap to get NLA relative frame.
 *       The caller should also check #BKE_fcurve_is_protected before keying.
 */
bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra);

}  // namespace blender::animrig
