/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * This interface allow GPU to manage GL objects for multiple context and threads.
 */

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::gpu {

typedef Vector<StringRef> DebugStack;

}  // namespace blender::gpu
