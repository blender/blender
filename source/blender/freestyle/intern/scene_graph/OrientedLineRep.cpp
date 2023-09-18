/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to display an oriented line representation.
 */

#include "OrientedLineRep.h"

#include "../system/BaseObject.h"

namespace Freestyle {

void OrientedLineRep::accept(SceneVisitor &v)
{
  Rep::accept(v);  // NOLINT(bugprone-parent-virtual-call), this seems to intentionally *not* call
                   // the parent class' accept() function, but rather the grandparent's. The
                   // v.visitLineRep(*this); call below is actually what the parent class would do.
  if (!frs_material()) {
    v.visitOrientedLineRep(*this);
  }
  else {
    v.visitLineRep(*this);
  }
}

} /* namespace Freestyle */
