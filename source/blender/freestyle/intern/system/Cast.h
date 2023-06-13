/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Cast function
 */

namespace Freestyle {

namespace Cast {
template<class T, class U> U *cast(T *in)
{
  if (!in) {
    return NULL;
  }
  return dynamic_cast<U *>(in);
}
}  // end of namespace Cast

} /* namespace Freestyle */
