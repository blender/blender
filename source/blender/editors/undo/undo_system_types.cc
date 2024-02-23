/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "ED_armature.hh"
#include "ED_curve.hh"
#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_lattice.hh"
#include "ED_mball.hh"
#include "ED_mesh.hh"
#include "ED_paint.hh"
#include "ED_particle.hh"
#include "ED_sculpt.hh"
#include "ED_text.hh"
#include "ED_undo.hh"
#include "undo_intern.hh"

/* Keep last */
#include "BKE_undo_system.hh"

void ED_undosys_type_init()
{
  /* Edit Modes */
  using namespace blender;
  BKE_undosys_type_append(ED_armature_undosys_type);
  BKE_undosys_type_append(ED_curve_undosys_type);
  BKE_undosys_type_append(ED_font_undosys_type);
  BKE_undosys_type_append(ED_lattice_undosys_type);
  BKE_undosys_type_append(ED_mball_undosys_type);
  BKE_undosys_type_append(ED_mesh_undosys_type);
  BKE_undosys_type_append(ED_curves_undosys_type);
  BKE_undosys_type_append(ED_undosys_type_grease_pencil);

  /* Paint Modes */
  BKE_UNDOSYS_TYPE_IMAGE = BKE_undosys_type_append(ED_image_undosys_type);

  BKE_UNDOSYS_TYPE_SCULPT = BKE_undosys_type_append(ed::sculpt_paint::undo::register_type);

  BKE_UNDOSYS_TYPE_PARTICLE = BKE_undosys_type_append(ED_particle_undosys_type);

  BKE_UNDOSYS_TYPE_PAINTCURVE = BKE_undosys_type_append(ED_paintcurve_undosys_type);

  /* Text editor */
  BKE_UNDOSYS_TYPE_TEXT = BKE_undosys_type_append(ED_text_undosys_type);

  /* Keep global undo last (as a fallback). */
  BKE_UNDOSYS_TYPE_MEMFILE = BKE_undosys_type_append(ED_memfile_undosys_type);
}

void ED_undosys_type_free()
{
  BKE_undosys_type_free_all();
}
