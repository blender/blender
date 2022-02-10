/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#if defined(WITH_OPENGL)
#  include "glew-mx.h"
#  ifndef WITH_LEGACY_OPENGL
#    include "GPU_legacy_stubs.h"
#  endif
#endif
