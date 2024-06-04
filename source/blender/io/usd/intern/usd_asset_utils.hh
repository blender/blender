/* SPDX-FileCopyrightText: 2023  NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include <string>

namespace blender::io::usd {

/**
 * Invoke the USD asset resolver to copy an asset.
 *
 * \param src: source path of the asset to copy
 * \param dst: destination path of the copy
 * \param name_collision_mode: behavior when `dst` already exists
 * \param reports: the storage for potential warning or error reports (generated using BKE_report
 *                 API).
 * \return true if the copy succeeded, false otherwise
 */
bool copy_asset(const char *src,
                const char *dst,
                eUSDTexNameCollisionMode name_collision_mode,
                ReportList *reports);

/**
 * Invoke the USD asset resolver to determine if the
 * asset with the given path exists.
 *
 * \param path: the path to resolve
 * \return true if the asset exists, false otherwise
 */
bool asset_exists(const char *path);

/**
 * Invoke the USD asset resolver to copy an asset to a destination
 * directory and return the path to the copied file.  This function may
 * be used to copy textures from a USDZ archive to a directory on disk.
 * The destination directory will be created if it doesn't already exist.
 * If the copy was unsuccessful, this function will log an error and
 * return the original source file path unmodified.
 *
 * \param src: source path of the asset to import
 * \param import_dir: path to the destination directory
 * \param name_collision_mode: behavior when a file of the same name already exists
 * \param reports: the storage for potential warning or error reports (generated using BKE_report
 *                 API).
 * \return path to copied file or the original `src` path if there was an error
 */
std::string import_asset(const char *src,
                         const char *import_dir,
                         eUSDTexNameCollisionMode name_collision_mode,
                         ReportList *reports);

/**
 * Check if the given path contains a UDIM token.
 *
 * \param path: the path to check
 * \return true if the path contains a UDIM token, false otherwise
 */
bool is_udim_path(const std::string &path);

/**
 * Returns path to temporary folder for saving imported textures prior to packing.
 * CAUTION: this directory is recursively deleted after material import.
 */
const char *temp_textures_dir();

}  // namespace blender::io::usd
