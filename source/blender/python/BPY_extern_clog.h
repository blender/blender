/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup python
 *
 * Logging defines.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* bpy_interface.cc */

extern struct CLG_LogRef *BPY_LOG_RNA;
extern struct CLG_LogRef *BPY_LOG_CONTEXT;

#ifdef __cplusplus
}
#endif
