/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_outliner_types.h"
#include "DNA_particle_types.h"

#include "../outliner_intern.hh"

#include "tree_element_particle_system.hh"

namespace blender::ed::outliner {

TreeElementParticleSystem::TreeElementParticleSystem(TreeElement &legacy_te,
                                                     Object & /* object */,
                                                     ParticleSystem &psys)
    : AbstractTreeElement(legacy_te), /* object_(object), */ psys_(psys)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LINKED_PSYS);
  legacy_te.directdata = &psys_;
  legacy_te.name = psys_.part->id.name + 2;
}

}  // namespace blender::ed::outliner
