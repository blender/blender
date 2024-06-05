/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_subprocess.hh"

#if defined(WITH_OPENGL_BACKEND) && defined(BLI_SUBPROCESS_SUPPORT)

void GPU_compilation_subprocess_run(const char *subprocess_name);

#endif
