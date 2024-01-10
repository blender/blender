/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef WITH_OPENVDB

#  include "BLI_vector.hh"

#  include "BKE_volume_grid.hh"

/**
 * The global volume grid file cache makes it easy to load volumes only once from disk and to then
 * reuse the loaded volume across Blender. Additionally, this also supports caching simplify
 * levels which are used when the "volume resolution" simplify scene setting is reduced. Working
 * with reduced resolution can improve performance and uses less memory.
 */
namespace blender::bke::volume_grid::file_cache {

/**
 * Get the volume grid identified by the parameters from a cache. This does not load the tree data
 * in grid because that is done on demand when it is accessed.
 */
GVolumeGrid get_grid_from_file(StringRef file_path, StringRef grid_name, int simplify_level = 0);

struct GridsFromFile {
  /**
   * Empty on success. Otherwise it contains information about why loading the file failed.
   */
  std::string error_message;
  /**
   * Meta data for the entire file (not for individual grids).
   */
  std::shared_ptr<openvdb::MetaMap> file_meta_data;
  /**
   * All grids stored in the file.
   */
  Vector<GVolumeGrid> grids;
};

/**
 * Get all the data stored in a `.vdb` file.
 * This does not actually load the tree data, which is done on demand.
 */
GridsFromFile get_all_grids_from_file(StringRef file_path, int simplify_level = 0);

/**
 * Remove all cached volume grids that are currently not referenced outside of the cache.
 */
void unload_unused();

}  // namespace blender::bke::volume_grid::file_cache

#endif
