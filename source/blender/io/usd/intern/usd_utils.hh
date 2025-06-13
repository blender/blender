/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "BLI_string_ref.hh"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>

#include <string>

namespace blender::io::usd {

/**
 * Return a valid USD identifier based on the passed in string.
 *
 * \param name: Incoming name to sanitize
 * \param allow_unicode: Whether to allow unicode encoded characters in the USD identifier
 * \return A valid USD identifier
 */
std::string make_safe_name(StringRef name, bool allow_unicode);

/* Return a unique USD `SdfPath`. If the given path already exists on the given stage, return
 * the path with a numerical suffix appended to the name that ensures the path is unique.
 * If the path does not exist on the stage, it will be returned unchanged.
 *
 * \param stage: The stage
 * \param path: The original path
 * \return A valid, and unique, USD `SdfPath`
 */
pxr::SdfPath get_unique_path(pxr::UsdStageRefPtr stage, const std::string &path);
}  // namespace blender::io::usd
