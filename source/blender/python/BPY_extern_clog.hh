/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup python
 *
 * Logging defines.
 */

#pragma once

struct CLG_LogRef;

namespace blender {

/* bpy_interface.cc */

extern CLG_LogRef *BPY_LOG_RNA;

}  // namespace blender
