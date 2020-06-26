/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree defining the simulation");

  /* common */
  rna_def_animdata_common(srna);
}

void RNA_def_simulation(BlenderRNA *brna)
{
  rna_def_simulation(brna);
}

#endif
