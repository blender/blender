/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "GridHelpers.h"

namespace Freestyle {

void GridHelpers::getDefaultViewProscenium(real viewProscenium[4])
{
  // Get proscenium boundary for culling
  // bufferZone determines the amount by which the area processed should exceed the actual image
  // area. This is intended to avoid visible artifacts generated along the proscenium edge. Perhaps
  // this is no longer needed now that entire view edges are culled at once, since that
  // theoretically should eliminate visible artifacts. To the extent it is still useful, bufferZone
  // should be put into the UI as configurable percentage value
  const real bufferZone = 0.05;
  // borderZone describes a blank border outside the proscenium, but still inside the image area.
  // Only intended for exposing possible artifacts along or outside the proscenium edge during
  // debugging.
  const real borderZone = 0.0;
  viewProscenium[0] = g_freestyle.viewport[2] * (borderZone - bufferZone);
  viewProscenium[1] = g_freestyle.viewport[2] * (1.0f - borderZone + bufferZone);
  viewProscenium[2] = g_freestyle.viewport[3] * (borderZone - bufferZone);
  viewProscenium[3] = g_freestyle.viewport[3] * (1.0f - borderZone + bufferZone);
}

GridHelpers::Transform::~Transform() = default;

} /* namespace Freestyle */
