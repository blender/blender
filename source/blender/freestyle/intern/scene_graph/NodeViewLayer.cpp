/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to represent a scene render layer in Blender.
 */

#include "NodeViewLayer.h"

namespace Freestyle {

void NodeViewLayer::accept(SceneVisitor &v)
{
  v.visitNodeViewLayer(*this);
}

} /* namespace Freestyle */
