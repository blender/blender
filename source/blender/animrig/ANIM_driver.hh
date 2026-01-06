/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with drivers.
 */

#include "RNA_types.hh"

namespace blender {

struct AnimationEvalContext;
struct FCurve;

namespace animrig {

/** Evaluates the driver on the frame given in `anim_eval_context` and returns the value. Returns 0
 * if the RNA path can't be resolved. */
float evaluate_driver_from_rna_pointer(const AnimationEvalContext *anim_eval_context,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const FCurve *fcu);

}  // namespace animrig
}  // namespace blender
