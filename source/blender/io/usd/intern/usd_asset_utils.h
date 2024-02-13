/* SPDX-FileCopyrightText: 2023  NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.h"

#include <pxr/usd/usd/stage.h>

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
 * Invoke the USD asset resolver to return an identifier for a 'textures' directory
 * which is a sibling of the given stage.  The resulting path is created by
 * resolving the './textures' relative path with the stage's root layer path as
 * the anchor.  If the parent of the stage root layer path resolves to a file
 * system path, the textures directory will be created, if it doesn't exist.
 *
 * \param stage: The stage whose root layer is a sibling of the 'textures'
 *               directory
 * \return the path to the 'textures' directory
 */
std::string get_export_textures_dir(const pxr::UsdStageRefPtr stage);

/**
 * Returns true if the parent directory of the given path exists on the
 * file system.
 *
 * \param path: input file path
 * \return true if the parent directory exists
 */
bool parent_dir_exists_on_file_system(const char *path);

/**
 * Return true if the asset at the given path is a candidate for importing
 * with the USD asset resolver.  The following heuristics are currently
 * applied for this test:
 * - Returns false if it's a Blender relative path.
 * - Returns true if the path is package-relative.
 * - Returns true is the path doesn't exist on the file system but can
 *   nonetheles be resolved by the USD asset resolver.
 * - Returns false otherwise.
 *
 * TODO(makowalski): the test currently requires a file-system stat.
 * Consider possible ways around this, e.g., by determining if the
 * path is a supported URI.
 *
 * \param path: input file path
 * \return true if the path should be imported, false otherwise
 */
bool should_import_asset(const std::string &path);

/**
 * Invokes the USD asset resolver to resolve the givn paths and
 * returns true if the resolved paths are equal.
 *
 * \param p1: first path to compare
 * \param p2: second path to compare
 * \return true if the resolved input paths are equal, returns
 *         false otherwise.
 *
 */
bool paths_equal(const char *p1, const char *p2);

/**
 * Returns path to temporary folder for saving imported textures prior to packing.
 * CAUTION: this directory is recursively deleted after material import.
 */
const char *temp_textures_dir();

/**
 * Invokes the USD asset resolver to write data to the given path.
 *
 * \param data: pointer to data to write
 * \param size: number of bytes to write
 * \param path: path of asset to be written
 * \param reports: the storage for potential warning or error reports (generated using BKE_report
 *                 API).
 * \return true if the data was written, returns
 *         false otherwise.
 *
 */
bool write_to_path(const void *data, size_t size, const char *path, ReportList *reports);

}  // namespace blender::io::usd
