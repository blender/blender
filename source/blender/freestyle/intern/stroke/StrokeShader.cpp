/* SPDX-FileCopyrightText: 2012-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "StrokeShader.h"

#include "../python/Director.h"

namespace Freestyle {

int StrokeShader::shade(Stroke &ioStroke) const
{
  return Director_BPy_StrokeShader_shade(const_cast<StrokeShader *>(this), ioStroke);
}

} /* namespace Freestyle */
