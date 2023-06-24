/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Creates a solid wireframe from connected faces.
 */

#include "DNA_material_types.h"

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "tools/bmesh_wireframe.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_wireframe_exec(BMesh *bm, BMOperator *op)
{
  const float offset = BMO_slot_float_get(op->slots_in, "thickness");
  const float offset_fac = BMO_slot_float_get(op->slots_in, "offset");
  const bool use_replace = BMO_slot_bool_get(op->slots_in, "use_replace");
  const bool use_boundary = BMO_slot_bool_get(op->slots_in, "use_boundary");
  const bool use_even_offset = BMO_slot_bool_get(op->slots_in, "use_even_offset");
  const bool use_relative_offset = BMO_slot_bool_get(op->slots_in, "use_relative_offset");
  const bool use_crease = BMO_slot_bool_get(op->slots_in, "use_crease");
  const float crease_weight = BMO_slot_float_get(op->slots_in, "crease_weight");

  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "faces", BM_FACE, BM_ELEM_TAG, false);

  BM_mesh_wireframe(bm,
                    offset,
                    offset_fac,
                    0.0f,
                    use_replace,
                    use_boundary,
                    use_even_offset,
                    use_relative_offset,
                    use_crease,
                    crease_weight,
                    /* dummy vgroup */
                    -1,
                    false,
                    0,
                    MAXMAT,
                    true);

  BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);
}
