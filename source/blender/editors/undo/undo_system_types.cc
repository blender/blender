/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_curves.h"
#include "ED_lattice.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_paint.h"
#include "ED_particle.h"
#include "ED_sculpt.h"
#include "ED_text.h"
#include "ED_undo.h"
#include "undo_intern.hh"

/* Keep last */
#include "BKE_undo_system.h"

void ED_undosys_type_init(void)
{
  /* Edit Modes */
  BKE_undosys_type_append(ED_armature_undosys_type);
  BKE_undosys_type_append(ED_curve_undosys_type);
  BKE_undosys_type_append(ED_font_undosys_type);
  BKE_undosys_type_append(ED_lattice_undosys_type);
  BKE_undosys_type_append(ED_mball_undosys_type);
  BKE_undosys_type_append(ED_mesh_undosys_type);
  BKE_undosys_type_append(ED_curves_undosys_type);

  /* Paint Modes */
  BKE_UNDOSYS_TYPE_IMAGE = BKE_undosys_type_append(ED_image_undosys_type);

  BKE_UNDOSYS_TYPE_SCULPT = BKE_undosys_type_append(ED_sculpt_undosys_type);

  BKE_UNDOSYS_TYPE_PARTICLE = BKE_undosys_type_append(ED_particle_undosys_type);

  BKE_UNDOSYS_TYPE_PAINTCURVE = BKE_undosys_type_append(ED_paintcurve_undosys_type);

  /* Text editor */
  BKE_UNDOSYS_TYPE_TEXT = BKE_undosys_type_append(ED_text_undosys_type);

  /* Keep global undo last (as a fallback). */
  BKE_UNDOSYS_TYPE_MEMFILE = BKE_undosys_type_append(ED_memfile_undosys_type);
}

void ED_undosys_type_free(void)
{
  BKE_undosys_type_free_all();
}
