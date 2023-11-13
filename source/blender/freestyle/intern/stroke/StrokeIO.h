/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Functions to manage I/O for the stroke
 */

#include <iostream>

#include "Stroke.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

ostream &operator<<(ostream &out, const StrokeAttribute &iStrokeAttribute);

ostream &operator<<(ostream &out, const StrokeVertex &iStrokeVertex);

ostream &operator<<(ostream &out, const Stroke &iStroke);

} /* namespace Freestyle */
