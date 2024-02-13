/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_driver.hh"
#include "BKE_fcurve_driver.h"
#include "DNA_anim_types.h"
#include "RNA_access.hh"

namespace blender::animrig {

float evaluate_driver_from_rna_pointer(const AnimationEvalContext *anim_eval_context,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const FCurve *fcu)
{
  PathResolvedRNA anim_rna;
  if (!RNA_path_resolved_create(ptr, prop, fcu->array_index, &anim_rna)) {
    return 0.0f;
  }
  return evaluate_driver(&anim_rna, fcu->driver, fcu->driver, anim_eval_context);
}

}  // namespace blender::animrig
