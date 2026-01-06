/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This file only exists to forward declare `ImplicitSharingInfo` in C code.
 */

namespace blender {

#ifdef __cplusplus

class ImplicitSharingInfo;
using ImplicitSharingInfoHandle = ImplicitSharingInfo;

#else

struct ImplicitSharingInfoHandle;

#endif

}  // namespace blender
