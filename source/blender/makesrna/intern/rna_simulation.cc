/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_simulation_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#else

static void rna_def_simulation(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Simulation", "ID");
  RNA_def_struct_ui_text(srna, "Simulation", "Simulation data-block");
  RNA_def_struct_ui_icon(srna, ICON_PHYSICS); /* TODO: Use correct icon. */

  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree defining the simulation");

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_simulation(BlenderRNA *brna)
{
  rna_def_simulation(brna);
}

#endif
