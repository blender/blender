/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \param path: The path to check against.
 * \return Success
 */
bool BKE_autoexec_match(const char *path);

#ifdef __cplusplus
}
#endif
