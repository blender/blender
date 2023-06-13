/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This file only exists to forward declare `blender::ImplicitSharingInfo` in C code.
 */

#ifdef __cplusplus

namespace blender {
class ImplicitSharingInfo;
}
using ImplicitSharingInfoHandle = blender::ImplicitSharingInfo;

#else

typedef struct ImplicitSharingInfoHandle ImplicitSharingInfoHandle;

#endif
