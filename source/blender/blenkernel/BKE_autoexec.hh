/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

/**
 * \param path: The path to check against.
 * \return Success
 */
bool BKE_autoexec_match(const char *path);

}  // namespace blender
