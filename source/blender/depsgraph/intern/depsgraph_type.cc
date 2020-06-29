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
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Defines and code for core node types.
 */

#include <cstdlib>  // for BLI_assert()

#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#include "DEG_depsgraph.h"

#include "intern/depsgraph_type.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_operation.h"

namespace deg = blender::deg;

/* Register all node types */
void DEG_register_node_types(void)
{
  /* register node types */
  deg::deg_register_base_depsnodes();
  deg::deg_register_component_depsnodes();
  deg::deg_register_operation_depsnodes();
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
}

deg::DEGCustomDataMeshMasks::DEGCustomDataMeshMasks(const CustomData_MeshMasks *other)
    : vert_mask(other->vmask),
      edge_mask(other->emask),
      face_mask(other->fmask),
      loop_mask(other->lmask),
      poly_mask(other->pmask)
{
}
