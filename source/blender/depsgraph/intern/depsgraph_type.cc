/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Defines and code for core node types.
 */

#include <cstdlib> /* for BLI_assert() */

#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#include "DEG_depsgraph.h"

#include "intern/depsgraph_type.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_operation.h"

namespace deg = blender::deg;

void DEG_register_node_types()
{
  /* register node types */
  deg::deg_register_base_depsnodes();
  deg::deg_register_component_depsnodes();
  deg::deg_register_operation_depsnodes();
}

void DEG_free_node_types() {}

deg::DEGCustomDataMeshMasks::DEGCustomDataMeshMasks(const CustomData_MeshMasks *other)
    : vert_mask(other->vmask),
      edge_mask(other->emask),
      face_mask(other->fmask),
      loop_mask(other->lmask),
      poly_mask(other->pmask)
{
}
