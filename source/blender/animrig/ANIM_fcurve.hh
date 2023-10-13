/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra);

}  // namespace blender::animrig
