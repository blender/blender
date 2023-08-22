/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define the representation of a vertex for displaying purpose.
 */

#include "VertexRep.h"

namespace Freestyle {

void VertexRep::ComputeBBox()
{
  setBBox(BBox<Vec3r>(Vec3r(_coordinates[0], _coordinates[1], _coordinates[2]),
                      Vec3r(_coordinates[0], _coordinates[1], _coordinates[2])));
}

} /* namespace Freestyle */
