/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_dbus_dynload
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool dbus_dynload_init(bool verbose);

#ifdef __cplusplus
}
#endif
